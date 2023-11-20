#include "common/font_helper.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "common/font_data.h"

using absl::flat_hash_map;
using common::FontData;

namespace common {

absl::StatusOr<absl::string_view> FontHelper::GlyfData(const hb_face_t* face,
                                                       uint32_t gid) {
  auto loca = Loca(face);
  if (!loca.ok()) {
    return loca.status();
  }

  FontData head = TableData(face, kHead);
  if (head.size() < 52) {
    return absl::InvalidArgumentError("invalid head table, too short.");
  }

  bool is_short_loca = !head.str()[51];
  uint32_t width = is_short_loca ? 2 : 4;
  uint32_t start_index = gid * width;
  uint32_t end_index = (gid + 1) * width;

  if (loca->size() < end_index + width) {
    return absl::InvalidArgumentError("invalid loca table, too short.");
  }

  uint32_t glyph_data_start = 0;
  uint32_t glyph_data_end = 0;
  if (is_short_loca) {
    auto glyph_start = ReadUInt16(loca->substr(start_index));
    auto glyph_end = ReadUInt16(loca->substr(end_index));
    if (!glyph_start.ok()) {
      return glyph_start.status();
    }
    if (!glyph_end.ok()) {
      return glyph_end.status();
    }
    glyph_data_start = *glyph_start * 2;
    glyph_data_end = *glyph_end * 2;
  } else {
    auto glyph_start = ReadUInt32(loca->substr(start_index));
    auto glyph_end = ReadUInt32(loca->substr(end_index));
    if (!glyph_start.ok()) {
      return glyph_start.status();
    }
    if (!glyph_end.ok()) {
      return glyph_end.status();
    }
    glyph_data_start = *glyph_start;
    glyph_data_end = *glyph_end;
  }

  if (glyph_data_end < glyph_data_start) {
    return absl::InvalidArgumentError(
        "invalid loca entry, end is less than start.");
  }

  auto glyf = TableData(face, kGlyf);
  if (glyf.size() < glyph_data_end) {
    return absl::InvalidArgumentError("invalid glyf table, too short.");
  }

  return glyf.str().substr(glyph_data_start, glyph_data_end - glyph_data_start);
}

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

void FontHelper::ApplyIftbTableOrdering(hb_face_t* subset) {
  std::vector<hb_tag_t> tags = GetOrderedTags(subset);
  std::vector<hb_tag_t> new_order;
  for (hb_tag_t t : tags) {
    if (t != FontHelper::kGlyf && t != FontHelper::kLoca &&
        t != FontHelper::kCFF && t != FontHelper::kCFF2 &&
        t != FontHelper::kGvar) {
      new_order.push_back(t);
    }
  }

  new_order.push_back(FontHelper::kCFF);
  new_order.push_back(FontHelper::kCFF2);
  new_order.push_back(FontHelper::kGvar);
  new_order.push_back(FontHelper::kGlyf);
  new_order.push_back(FontHelper::kLoca);
  new_order.push_back(0);
  hb_face_builder_sort_tables(subset, new_order.data());
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