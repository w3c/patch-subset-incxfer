#include "common/font_helper.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"

using absl::flat_hash_map;

namespace common {

flat_hash_map<uint32_t, uint32_t> FontHelper::GidToUnicodeMap(hb_face_t* face) {
  hb_map_t* unicode_to_gid = hb_map_create();
  hb_face_collect_nominal_glyph_mapping(face, unicode_to_gid, nullptr);

  flat_hash_map<uint32_t, uint32_t> gid_to_unicode;
  int index = -1;
  uint32_t cp = HB_MAP_VALUE_INVALID;
  uint32_t gid = HB_MAP_VALUE_INVALID;
  while (hb_map_next(unicode_to_gid, &index, &cp, &gid)) {
    gid_to_unicode[gid] = cp;
  }

  hb_map_destroy(unicode_to_gid);
  return gid_to_unicode;
}

absl::flat_hash_set<hb_tag_t> FontHelper::GetTags(hb_face_t* face) {
  absl::flat_hash_set<hb_tag_t> tag_set;
  constexpr uint32_t max_tags = 64;
  hb_tag_t table_tags[max_tags];
  unsigned table_count = max_tags;
  unsigned offset = 0;

  while (((void)hb_face_get_table_tags(face, offset, &table_count, table_tags),
          table_count)) {
    for (unsigned i = 0; i < table_count; i++) {
      hb_tag_t tag = table_tags[i];
      tag_set.insert(tag);
    }
    offset += table_count;
  }
  return tag_set;
}

std::vector<hb_tag_t> FontHelper::GetOrderedTags(hb_face_t* face) {
  std::vector<hb_tag_t> ordered_tags;
  auto tags = GetTags(face);

  std::copy(tags.begin(), tags.end(), std::back_inserter(ordered_tags));
  std::sort(ordered_tags.begin(), ordered_tags.end(),
            CompareTableOffsets(face));

  return ordered_tags;
}

std::string FontHelper::ToString(hb_tag_t tag) {
  std::string tag_name;
  tag_name.resize(5);  // need 5 for the null terminator
  snprintf(tag_name.data(), 5, "%c%c%c%c", HB_UNTAG(tag));
  tag_name.resize(4);
  return tag_name;
}

std::vector<std::string> FontHelper::ToStrings(
    const std::vector<hb_tag_t>& tags) {
  std::vector<std::string> result;
  for (hb_tag_t tag : tags) {
    result.push_back(ToString(tag));
  }

  return result;
}

}  // namespace common