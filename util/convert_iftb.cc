#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/font_data.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StatusOr;
using absl::string_view;
using common::FontData;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchMap;

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

std::vector<uint32_t> load_id(string_view line) {
  std::vector<uint32_t> result;
  std::string next;
  while (!line.empty()) {
    line = next_token(line, " ", next);
    result.push_back(std::stoul(next, 0, 16));
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

StatusOr<IFTTable> create_table(
    const std::string& url_template, const std::vector<uint32_t>& id,
    const flat_hash_map<std::uint32_t, uint32_t>& gid_map,
    const flat_hash_set<uint32_t>& loaded_chunks, hb_face_t* face) {
  btree_map<uint32_t, btree_set<uint32_t>> chunk_to_codepoints =
      compress_gid_map(gid_map, loaded_chunks, face);

  IFTTable ift;
  ift.SetUrlTemplate(url_template);
  auto s = ift.SetId(Span<const uint32_t>(id.data(), id.size()));
  if (!s.ok()) {
    return s;
  }

  for (auto e : chunk_to_codepoints) {
    PatchMap::Coverage coverage;
    std::copy(e.second.begin(), e.second.end(),
              std::inserter(coverage.codepoints, coverage.codepoints.begin()));
    ift.GetPatchMap().AddEntry(coverage, e.first, IFTB_ENCODING);
  }

  // TODO(garretrieger): populate the additional fields.
  return ift;
}

StatusOr<IFTTable> convert_iftb(string_view iftb_dump, hb_face_t* face) {
  flat_hash_map<std::uint32_t, uint32_t> gid_map;
  flat_hash_set<uint32_t> loaded_chunks;
  std::vector<uint32_t> id;
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

    if (field == "ID") {
      id = load_id(line);
    }
  }

  fprintf(stderr, "Font ID = %x %x %x %x\n", id[0], id[1], id[2], id[3]);
  return create_table(url_template, id, gid_map, loaded_chunks, face);
}

}  // namespace util
