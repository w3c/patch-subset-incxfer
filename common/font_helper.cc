#include "common/font_helper.h"

#include <cstdint>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "common/axis_range.h"
#include "common/fast_hasher.h"
#include "common/font_data.h"
#include "common/indexed_data_reader.h"
#include "hb-ot.h"
#include "hb-subset.h"
#include "hb.h"

using absl::btree_set;
using absl::flat_hash_map;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::FontData;

namespace common {

absl::StatusOr<string_view> FontHelper::GlyfData(const hb_face_t* face,
                                                 uint32_t gid) {
  auto loca = Loca(face);
  if (!loca.ok()) {
    return loca.status();
  }

  FontData head = TableData(face, kHead);
  if (head.size() < 52) {
    return absl::InvalidArgumentError("invalid head table, too short.");
  }

  auto glyf = TableData(face, kGlyf);
  bool is_short_loca = !head.str()[51];
  if (is_short_loca) {
    IndexedDataReader<uint16_t, 2> reader(*loca, glyf.str());
    return reader.DataFor(gid);
  } else {
    IndexedDataReader<uint32_t, 1> reader(*loca, glyf.str());
    return reader.DataFor(gid);
  }
}

StatusOr<string_view> FontHelper::GvarData(const hb_face_t* face,
                                           uint32_t gid) {
  auto gvar = TableData(face, kGvar);
  if (gvar.empty()) {
    return absl::NotFoundError("gvar not in the font.");
  }

  constexpr uint32_t glyph_count_offset = 12;
  constexpr uint32_t gvar_flags_offset = 15;
  constexpr uint32_t data_array_offset = 16;
  constexpr uint32_t gvar_offsets_table_offset = 20;

  if (gvar.size() < 20) {
    return absl::InvalidArgumentError("gvar table is too short.");
  }

  auto glyph_count = ReadUInt16(gvar.str().substr(glyph_count_offset));
  if (!glyph_count.ok()) {
    return glyph_count.status();
  }

  auto data_offset = ReadUInt32(gvar.str().substr(data_array_offset));
  if (!data_offset.ok()) {
    return data_offset.status();
  }

  bool is_wide = (((uint8_t)gvar.str()[gvar_flags_offset]) & 0x01);
  if (is_wide) {
    IndexedDataReader<uint32_t, 1> reader(
        gvar.str().substr(gvar_offsets_table_offset, (*glyph_count + 1) * 4),
        gvar.str().substr(*data_offset));
    return reader.DataFor(gid);
  }

  IndexedDataReader<uint16_t, 2> reader(
      gvar.str().substr(gvar_offsets_table_offset, (*glyph_count + 1) * 2),
      gvar.str().substr(*data_offset));
  return reader.DataFor(gid);
}

StatusOr<uint32_t> FontHelper::GvarSharedTupleCount(const hb_face_t* face) {
  auto gvar = TableData(face, kGvar);
  if (gvar.empty()) {
    return absl::NotFoundError("gvar not in the font.");
  }

  constexpr uint32_t shared_tuple_count_offset = 6;

  if (gvar.size() < 8) {
    return absl::InvalidArgumentError("gvar table is too short.");
  }

  auto count = ReadUInt16(gvar.str().substr(shared_tuple_count_offset));
  if (!count.ok()) {
    return count.status();
  }

  return *count;
}

StatusOr<uint64_t> FontHelper::GvarSharedTupleChecksum(hb_face_t* face) {
  auto gvar = TableData(face, kGvar);
  if (gvar.empty()) {
    return absl::NotFoundError("gvar not in the font.");
  }

  constexpr uint32_t shared_tuple_offset = 8;

  if (gvar.size() < 12) {
    return absl::InvalidArgumentError("gvar table is too short.");
  }

  auto offset = ReadUInt32(gvar.str().substr(shared_tuple_offset));
  if (!offset.ok()) {
    return offset.status();
  }

  uint16_t shared_tuple_count = *GvarSharedTupleCount(face);
  uint32_t axis_count = hb_ot_var_get_axis_count(face);
  uint32_t shared_tuple_length = axis_count * shared_tuple_count * 2;
  if (gvar.size() < *offset + shared_tuple_length) {
    return absl::InvalidArgumentError("gvar table is too short.");
  }

  string_view shared_tuple_data =
      gvar.str().substr(*offset, shared_tuple_length);
  FastHasher hasher;
  return hasher.Checksum(shared_tuple_data);
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

void GetFeatureTagsFrom(hb_face_t* face, hb_tag_t table,
                        btree_set<hb_tag_t>& tag_set) {
  constexpr uint32_t max_tags = 32;
  hb_tag_t feature_tags[max_tags];
  unsigned tag_count = max_tags;
  unsigned offset = 0;

  while (((void)hb_ot_layout_table_get_feature_tags(face, table, offset,
                                                    &tag_count, feature_tags),
          tag_count)) {
    for (unsigned i = 0; i < tag_count; i++) {
      hb_tag_t tag = feature_tags[i];
      tag_set.insert(tag);
    }
    offset += tag_count;
  }
}

btree_set<hb_tag_t> FontHelper::GetFeatureTags(hb_face_t* face) {
  btree_set<hb_tag_t> tag_set;
  GetFeatureTagsFrom(face, kGSUB, tag_set);
  GetFeatureTagsFrom(face, kGPOS, tag_set);
  return tag_set;
}

absl::btree_set<hb_tag_t> FontHelper::GetNonDefaultFeatureTags(
    hb_face_t* face) {
  auto tag_set = GetFeatureTags(face);

  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  hb_set_t* default_tags =
      hb_subset_input_set(input, HB_SUBSET_SETS_LAYOUT_FEATURE_TAG);
  hb_tag_t tag = HB_SET_VALUE_INVALID;
  while (hb_set_next(default_tags, &tag)) {
    tag_set.erase(tag);
  }
  hb_subset_input_destroy(input);

  return tag_set;
}

StatusOr<flat_hash_map<hb_tag_t, AxisRange>> FontHelper::GetDesignSpace(
    hb_face_t* face) {
  constexpr uint32_t max_axes = 32;
  hb_ot_var_axis_info_t axes[max_axes];
  unsigned axes_count = max_axes;
  unsigned offset = 0;

  flat_hash_map<hb_tag_t, AxisRange> result;

  while (((void)hb_ot_var_get_axis_infos(face, offset, &axes_count, axes),
          axes_count)) {
    for (unsigned i = 0; i < axes_count; i++) {
      auto axis = axes[i];
      auto r = AxisRange::Range(axis.min_value, axis.max_value);
      if (!r.ok()) {
        return r.status();
      }
      result[axis.tag] = *r;
    }
    offset += axes_count;
  }

  return result;
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

hb_tag_t FontHelper::ToTag(const std::string& tag) {
  return HB_TAG(tag[0], tag[1], tag[2], tag[3]);
}

std::vector<std::string> FontHelper::ToStrings(
    const std::vector<hb_tag_t>& tags) {
  std::vector<std::string> result;
  for (hb_tag_t tag : tags) {
    result.push_back(ToString(tag));
  }
  return result;
}

std::vector<std::string> FontHelper::ToStrings(
    const btree_set<hb_tag_t>& input) {
  std::vector<std::string> result;
  for (hb_tag_t tag : input) {
    result.push_back(ToString(tag));
  }
  return result;
}

}  // namespace common