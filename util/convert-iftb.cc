#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hb.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

ABSL_FLAG(bool, text_format, false, "Output the table in text format.");

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using google::protobuf::TextFormat;

size_t next_token(const std::string& line, const std::string& delim,
                  size_t prev, std::string& out) {
  if (prev == line.size()) {
    return std::string::npos;
  }

  size_t index = line.find(delim, prev);
  if (index == std::string::npos) {
    out = line.substr(prev);
    return line.size();
  }

  out = line.substr(prev, index - prev);
  return index + delim.size();
}

hb_face_t* load_font(const char* filename) {
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", filename);
    exit(-1);
  }

  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  return face;
}

flat_hash_set<std::uint32_t> load_chunk_set(const std::string& line,
                                            size_t index) {
  flat_hash_set<std::uint32_t> result;

  std::string next;
  while ((index = next_token(line, ", ", index, next)) != std::string::npos) {
    result.insert(std::stoi(next));
  }

  return result;
}

flat_hash_map<std::uint32_t, uint32_t> load_gid_map(const std::string& line,
                                                    size_t index) {
  flat_hash_map<std::uint32_t, uint32_t> result;

  std::string next;
  while ((index = next_token(line, ", ", index, next)) != std::string::npos) {
    size_t inner_index = 0;
    std::string gid;
    std::string chunk;

    inner_index = next_token(next, ":", inner_index, gid);
    inner_index = next_token(next, ":", inner_index, chunk);

    result[std::stoi(gid)] = std::stoi(chunk);
  }

  return result;
}

flat_hash_map<uint32_t, flat_hash_set<uint32_t>> load_gid_to_unicode_map(
    hb_face_t* face) {
  hb_map_t* unicode_to_gid = hb_map_create();
  hb_set_t* unicodes = hb_set_create();
  hb_face_collect_nominal_glyph_mapping(face, unicode_to_gid, unicodes);

  flat_hash_map<uint32_t, flat_hash_set<uint32_t>> gid_to_unicodes;

  int index = -1;
  hb_codepoint_t cp;
  hb_codepoint_t gid;
  while (hb_map_next(unicode_to_gid, &index, &cp, &gid)) {
    gid_to_unicodes[gid].insert(cp);
  }

  hb_map_destroy(unicode_to_gid);
  hb_set_destroy(unicodes);

  return gid_to_unicodes;
}

btree_map<uint32_t, btree_set<uint32_t>> compress_gid_map(
    const flat_hash_map<std::uint32_t, uint32_t>& gid_map,
    const flat_hash_set<uint32_t>& loaded_chunks, hb_face_t* face) {
  flat_hash_map<uint32_t, flat_hash_set<uint32_t>> gid_to_unicodes =
      load_gid_to_unicode_map(face);
  btree_map<uint32_t, btree_set<uint32_t>> result;

  for (auto e : gid_map) {
    uint32_t gid = e.first;
    uint32_t chunk = e.second;
    if (!chunk || loaded_chunks.contains(chunk)) {
      // if chunk is loaded we don't need to map it.
      continue;
    }

    if (gid_to_unicodes[gid].empty()) {
      fprintf(stderr, "WARNING: gid %u not found in cmap.\n", gid);
    }

    for (uint32_t cp : gid_to_unicodes[gid]) {
      result[chunk].insert(cp);
    }
  }

  return result;
}

void to_subset_mapping(uint32_t chunk, btree_set<uint32_t> codepoints,
                       SubsetMapping* mapping) {
  mapping->set_id(chunk);

  auto it = codepoints.begin();
  uint32_t lowest = *it;

  hb_set_t* biased_codepoints = hb_set_create();
  for (uint32_t cp : codepoints) {
    if (lowest > cp) {
      fprintf(stderr, "FATAL: %u > %u.", lowest, cp);
      exit(-1);
    }
    hb_set_add(biased_codepoints, cp - lowest);
  }

  std::string encoded = patch_subset::SparseBitSet::Encode(*biased_codepoints);
  hb_set_destroy(biased_codepoints);

  mapping->set_bias(lowest);
  mapping->set_codepoint_set(encoded);
}

IFT create_table(const std::string& url_template,
                 const flat_hash_map<std::uint32_t, uint32_t>& gid_map,
                 const flat_hash_set<uint32_t>& loaded_chunks,
                 hb_face_t* face) {
  btree_map<uint32_t, btree_set<uint32_t>> chunk_to_codepoints =
      compress_gid_map(gid_map, loaded_chunks, face);

  IFT ift;
  ift.set_url_template(url_template);

  for (auto e : chunk_to_codepoints) {
    to_subset_mapping(e.first, e.second, ift.add_subset_mapping());
  }

  // TODO(garretrieger): populate the additional fields.

  return ift;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() != 2) {
    printf("usage: [--notext_format] <path to font>\n");
    return -1;
  }

  hb_face_t* face = load_font(args[1]);

  std::string line;

  flat_hash_map<std::uint32_t, uint32_t> gid_map;
  flat_hash_set<uint32_t> loaded_chunks;
  std::string url_template;

  while (std::getline(std::cin, line)) {
    size_t index = 0;
    std::string next;

    if ((index = next_token(line, ": ", index, next)) == std::string::npos) {
      continue;
    }

    fprintf(stderr, ">> %s\n", next.c_str());

    if (next == "gidMap") {
      gid_map = load_gid_map(line, index);
      continue;
    }

    if (next == "chunkSet indexes") {
      loaded_chunks = load_chunk_set(line, index);
      continue;
    }

    if (next == "filesURI") {
      url_template = line.substr(index);
      continue;
    }
  }

  IFT ift = create_table(url_template, gid_map, loaded_chunks, face);

  if (absl::GetFlag(FLAGS_text_format)) {
    std::string out;
    TextFormat::PrintToString(ift, &out);
    std::cout << out << std::endl;
  } else {
    std::string out;
    std::cout << ift.SerializeAsString();
  }

  hb_face_destroy(face);
  return 0;
}