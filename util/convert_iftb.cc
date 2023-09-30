#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "hb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::string_view;
using patch_subset::FontData;

namespace util {

string_view next_token(string_view line, string_view delim, std::string& out) {
  if (line.empty()) {
    out = "";
    return line;
  }

  size_t index = line.find(delim);
  if (index == std::string::npos) {
    out = line;
    return string_view();
  }

  out = line.substr(0, index);
  return line.substr(index + delim.size());
}

flat_hash_set<std::uint32_t> load_chunk_set(string_view line) {
  flat_hash_set<std::uint32_t> result;

  std::string next;
  while (!line.empty()) {
    line = next_token(line, ", ", next);
    result.insert(std::stoi(next));
  }

  return result;
}

flat_hash_map<std::uint32_t, uint32_t> load_gid_map(string_view line) {
  flat_hash_map<std::uint32_t, uint32_t> result;

  std::string next;
  while (!line.empty()) {
    line = next_token(line, ", ", next);

    std::string gid;
    std::string chunk;
    next = next_token(next, ":", gid);
    next = next_token(next, ":", chunk);

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

IFT convert_iftb(string_view iftb_dump, hb_face_t* face) {
  flat_hash_map<std::uint32_t, uint32_t> gid_map;
  flat_hash_set<uint32_t> loaded_chunks;
  std::string url_template;

  while (!iftb_dump.empty()) {
    std::string line;
    iftb_dump = next_token(iftb_dump, "\n", line);

    std::string field;
    line = next_token(line, ": ", field);

    fprintf(stderr, ">> %s\n", field.c_str());

    if (field == "gidMap") {
      gid_map = load_gid_map(line);
      continue;
    }

    if (field == "chunkSet indexes") {
      loaded_chunks = load_chunk_set(line);
      continue;
    }

    if (field == "filesURI") {
      if (line.size() && line[line.size() - 1] == 0) {
        line = line.substr(0, line.size() - 1);
      }
      url_template = line;
      continue;
    }
  }

  return create_table(url_template, gid_map, loaded_chunks, face);
}

}  // namespace util