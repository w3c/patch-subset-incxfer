#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "hb.h"
#include "patch_subset/proto/IFT.pb.h"

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
    printf("failed to load file: %s\n", filename);
    exit(-1);
  }

  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  return face;
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

hb_map_t* load_gid_to_unicode_map(hb_face_t* face) {
  hb_map_t* unicode_to_gid = hb_map_create();
  hb_set_t* unicodes = hb_set_create();
  hb_face_collect_nominal_glyph_mapping(face, unicode_to_gid, unicodes);

  hb_map_t* gid_to_unicode = hb_map_create();
  int index = -1;
  hb_codepoint_t cp;
  hb_codepoint_t gid;
  while (hb_map_next(unicode_to_gid, &index, &cp, &gid)) {
    if (hb_map_has(gid_to_unicode, gid)) {
      // TODO(garretrieger): support this. Need to map from gid -> {codepoint
      // set}.
      printf("WARNING: multi codepoints map to the same gid (%u)\n", gid);
    }
    hb_map_set(gid_to_unicode, gid, cp);
  }

  hb_map_destroy(unicode_to_gid);
  hb_set_destroy(unicodes);

  return gid_to_unicode;
}

btree_map<uint32_t, btree_set<uint32_t>> compress_gid_map(
    const flat_hash_map<std::uint32_t, uint32_t>& gid_map, hb_face_t* face) {
  // TODO(garretrieger): don't include mappings in the compressed table for
  // glyphs/codepoints that are
  //                     already loaded into the font.

  hb_map_t* gid_to_unicode = load_gid_to_unicode_map(face);
  btree_map<uint32_t, btree_set<uint32_t>> result;

  for (auto e : gid_map) {
    uint32_t gid = e.first;
    uint32_t chunk = e.second;
    uint32_t cp = hb_map_get(gid_to_unicode, gid);
    if (cp == (unsigned)-1) {
      // TODO(garretrieger): this can be ignored if the associated chunk is
      // already loaded.
      printf("WARNING: gid %u not found in cmap.\n", gid);
    }

    result[chunk].insert(cp);
  }

  hb_map_destroy(gid_to_unicode);

  return result;
}

IFT create_table(const flat_hash_map<std::uint32_t, uint32_t>& gid_map,
                 hb_face_t* face) {
  btree_map<uint32_t, btree_set<uint32_t>> chunk_to_codepoints =
      compress_gid_map(gid_map, face);

  IFT ift;

  return ift;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("usage: <path to font>\n");
    return -1;
  }

  hb_face_t* face = load_font(argv[1]);

  std::string line;

  flat_hash_map<std::uint32_t, uint32_t> gid_map;

  while (std::getline(std::cin, line)) {
    size_t index = 0;
    std::string next;

    if ((index = next_token(line, ": ", index, next)) == std::string::npos) {
      continue;
    }

    printf(">> %s\n", next.c_str());

    if (next == "gidMap") {
      gid_map = load_gid_map(line, index);
      continue;
    }
  }

  IFT ift = create_table(gid_map, face);

  hb_face_destroy(face);
  return 0;
}