#include "ift/proto/format_2_patch_map.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/axis_range.h"
#include "common/compat_id.h"
#include "common/font_helper.h"
#include "common/font_helper_macros.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_encoding.h"
#include "ift/proto/patch_map.h"

using absl::ClippedSubstr;
using absl::Span;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::CompatId;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;

namespace ift::proto {

constexpr uint8_t features_and_design_space_bit_mask = 1;
constexpr uint8_t copy_mappings_bit_mask = 1 << 1;
constexpr uint8_t index_delta_bit_mask = 1 << 2;
constexpr uint8_t encoding_bit_mask = 1 << 3;
constexpr uint8_t codepoint_bit_mask = 0b11 << 4;
constexpr uint8_t ignore_bit_mask = 1 << 6;

constexpr uint8_t no_bias = 0b01 << 4;
constexpr uint8_t two_byte_bias = 0b10 << 4;
constexpr uint8_t three_byte_bias = 0b11 << 4;

static StatusOr<uint8_t> EncodingToInt(PatchEncoding encoding) {
  if (encoding == TABLE_KEYED_FULL) {
    return 1;
  }

  if (encoding == TABLE_KEYED_PARTIAL) {
    return 2;
  }

  if (encoding == GLYPH_KEYED) {
    return 3;
  }

  return absl::InvalidArgumentError(
      StrCat("Unknown patch encoding, ", encoding));
}

static StatusOr<PatchEncoding> IntToEncoding(uint8_t value) {
  switch (value) {
    case 1:
      return TABLE_KEYED_FULL;
    case 2:
      return TABLE_KEYED_PARTIAL;
    case 3:
      return GLYPH_KEYED;
      // fall through.
    default:
      return absl::InvalidArgumentError("Unrecognized encoding value.");
  }
}

static PatchEncoding PickDefaultEncoding(const PatchMap& patch_map) {
  uint32_t counts[4] = {0, 0, 0};
  for (const auto& e : patch_map.GetEntries()) {
    auto i = EncodingToInt(e.encoding);
    if (!i.ok()) {
      // Ignore.
      continue;
    }
    counts[*i]++;
  }

  if (counts[1] >= counts[2] && counts[1] >= counts[3]) {
    return TABLE_KEYED_FULL;
  }

  if (counts[2] >= counts[1] && counts[2] >= counts[3]) {
    return TABLE_KEYED_PARTIAL;
  }

  return GLYPH_KEYED;
}

static uint32_t NumEntries(const PatchMap& map) {
  return map.GetEntries().length();
}

// Decides whether to use 0, 1, or 2 bytes of bias.
static uint8_t BiasBytes(const PatchMap::Coverage& coverage);

static uint8_t BiasBytes(uint8_t format);

// Returns the two bit format used for the given number of bias bytes.
static uint8_t BiasFormat(uint8_t bias_bytes);

static Status EncodeAxisSegment(hb_tag_t tag, const common::AxisRange& range,
                                std::string& out);

static Status EncodeEntries(Span<const PatchMap::Entry> entries,
                            PatchEncoding default_encoding, std::string& out);

static Status EncodeEntry(const PatchMap::Entry& entry,
                          uint32_t last_entry_index,
                          PatchEncoding default_encoding, std::string& out);

static void EncodeCodepoints(uint8_t bias_bytes,
                             const PatchMap::Coverage& coverage,
                             std::string& out);

static Status DecodeAxisSegment(absl::string_view data, hb_tag_t& tag,
                                common::AxisRange& range);

static Status DecodeEntries(absl::string_view data, uint16_t count,
                            PatchEncoding default_encoding, PatchMap& out);

static StatusOr<absl::string_view> DecodeEntry(absl::string_view data,
                                               PatchEncoding default_encoding,
                                               uint32_t& entry_index,
                                               PatchMap& out);

StatusOr<std::string> Format2PatchMap::Serialize(const IFTTable& ift_table) {
  // TODO(garretrieger): pre-reserve estimated capacity based on patch_map.
  std::string out;
  const auto& patch_map = ift_table.GetPatchMap();

  FontHelper::WriteUInt8(0x02, out);  // Format = 2
  FontHelper::WriteUInt32(0x0, out);  // Reserved = 0x00000000

  // id
  ift_table.GetId().WriteTo(out);

  // defaultPatchEncoding
  PatchEncoding default_encoding = PickDefaultEncoding(patch_map);
  auto encoding_value = EncodingToInt(default_encoding);
  if (!encoding_value.ok()) {
    return encoding_value.status();
  }
  FontHelper::WriteUInt8(*encoding_value, out);

  // mappingCount
  WRITE_UINT24(NumEntries(patch_map), out,
               "Exceeded maximum number of entries (0xFFFFFF).");

  // entries offset
  string_view uri_template = ift_table.GetUrlTemplate();
  constexpr int header_min_length = 35;
  FontHelper::WriteUInt32(header_min_length + uri_template.length(), out);

  // idStrings
  FontHelper::WriteUInt32(0, out);

  // uriTemplateLength
  WRITE_UINT16(uri_template.length(), out,
               "Exceeded maximum uri template size (0xFFFF)");

  // uriTemplate
  out.append(uri_template);

  // entries
  auto s = EncodeEntries(patch_map.GetEntries(), default_encoding, out);
  if (!s.ok()) {
    return s;
  }

  return out;
}

Status DecodeAxisSegment(absl::string_view data, hb_tag_t& tag,
                         common::AxisRange& range) {
  READ_UINT32(tag_v, data, 0);
  tag = tag_v;

  READ_FIXED(start, data, 4);
  READ_FIXED(end, data, 8);

  auto r = common::AxisRange::Range(start, end);
  if (!r.ok()) {
    return r.status();
  }

  range = *r;
  return absl::OkStatus();
}

Status DecodeEntries(absl::string_view data, uint16_t count,
                     PatchEncoding default_encoding, PatchMap& out) {
  uint32_t entry_index = 0;
  for (uint32_t i = 0; i < count; i++) {
    auto s = DecodeEntry(data, default_encoding, entry_index, out);
    if (!s.ok()) {
      return s.status();
    }
    data = *s;
  }
  return absl::OkStatus();
}

StatusOr<absl::string_view> DecodeEntry(absl::string_view data,
                                        PatchEncoding default_encoding,
                                        uint32_t& entry_index, PatchMap& out) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Not enough input data to decode mapping entry.");
  }

  PatchMap::Coverage coverage;

  READ_UINT8(format, data, 0);
  uint32_t offset = 1;

  if (format & features_and_design_space_bit_mask) {
    READ_UINT8(feature_count, data, offset++);
    for (unsigned i = 0; i < feature_count; i++) {
      READ_UINT32(next_tag, data, offset);
      coverage.features.insert(next_tag);
      offset += 4;
    }
    READ_UINT16(segment_count, data, offset);
    offset += 2;
    for (uint16_t i = 0; i < segment_count; i++) {
      hb_tag_t tag;
      common::AxisRange range;
      auto s = DecodeAxisSegment(data.substr(offset), tag, range);
      if (!s.ok()) {
        return s;
      }
      coverage.design_space[tag] = range;
      offset += 12;
    }
  }

  if (format & copy_mappings_bit_mask) {
    READ_UINT16(copy_count, data, offset);
    offset += 2 + copy_count * 2;
    // TODO(garretrieger): read in copies
  }

  entry_index++;
  if (format & index_delta_bit_mask) {
    READ_INT16(delta, data, offset);
    entry_index += delta;
    offset += 2;
  }

  if (format & encoding_bit_mask) {
    READ_UINT8(encoding_int, data, offset++);
    auto new_encoding = IntToEncoding(encoding_int);
    if (!new_encoding.ok()) {
      return new_encoding.status();
    }
    default_encoding = *new_encoding;
  }

  if (format & codepoint_bit_mask) {
    uint8_t bias_bytes = BiasBytes(format);
    uint32_t bias = 0;
    if (bias_bytes == 2) {
      READ_UINT16(bias_val, data, offset);
      bias = bias_val;
      offset += bias_bytes;
    }

    if (bias_bytes == 3) {
      READ_UINT24(bias_val, data, offset);
      bias = bias_val;
      offset += bias_bytes;
    }

    hb_set_unique_ptr codepoint_set = make_hb_set();
    auto s = SparseBitSet::Decode(data.substr(offset), codepoint_set.get());
    if (!s.ok()) {
      return s;
    }

    data = *s;
    offset = 0;

    hb_codepoint_t cp = HB_SET_VALUE_INVALID;
    while (hb_set_next(codepoint_set.get(), &cp)) {
      coverage.codepoints.insert(cp + bias);
    }
  }

  if (!(format & ignore_bit_mask)) {
    out.AddEntry(coverage, entry_index, default_encoding);
  }

  return data.substr(offset);
}

Status EncodeAxisSegment(hb_tag_t tag, const common::AxisRange& range,
                         std::string& out) {
  FontHelper::WriteUInt32(tag, out);
  WRITE_FIXED(range.start(), out, "range.start() overflowed.");
  WRITE_FIXED(range.end(), out, "range.end() overflowed.");
  return absl::OkStatus();
}

Status EncodeEntries(Span<const PatchMap::Entry> entries,
                     PatchEncoding default_encoding, std::string& out) {
  // TODO(garretrieger): identify and copy existing entries when possible.
  uint32_t last_entry_index = 0;
  for (const auto& entry : entries) {
    auto s = EncodeEntry(entry, last_entry_index, default_encoding, out);
    if (!s.ok()) {
      return s;
    }
    last_entry_index = entry.patch_index;
  }

  return absl::OkStatus();
}

// Decides whether to use 0, 1, or 2 bytes of bias.
uint8_t BiasBytes(const PatchMap::Coverage& coverage) {
  uint8_t bias_bytes[3] = {0, 2, 3};
  uint8_t result = 0;
  size_t min = (size_t)-1;
  for (int i = 0; i < 3; i++) {
    std::string out;
    EncodeCodepoints(bias_bytes[i], coverage, out);
    if (out.size() < min) {
      min = out.size();
      result = bias_bytes[i];
    }
  }

  return result;
}

static uint8_t BiasBytes(uint8_t format) {
  if ((format & codepoint_bit_mask) == two_byte_bias) {
    return 2;
  }

  if ((format & codepoint_bit_mask) == three_byte_bias) {
    return 3;
  }

  return 0;
}

void EncodeCodepoints(uint8_t bias_bytes, const PatchMap::Coverage& coverage,
                      std::string& out) {
  uint32_t max_bias = (1 << ((uint32_t)bias_bytes) * 8) - 1;
  uint32_t bias = std::min(coverage.SmallestCodepoint(), max_bias);

  hb_set_unique_ptr biased_set = make_hb_set();
  for (uint32_t cp : coverage.codepoints) {
    hb_set_add(biased_set.get(), cp - bias);
  }

  std::string sparse_bit_set = SparseBitSet::Encode(*biased_set);

  if (bias_bytes == 2) {
    FontHelper::WriteUInt16(bias, out);
  } else if (bias_bytes == 3) {
    FontHelper::WriteUInt24(bias, out);
  }
  out.append(sparse_bit_set);
}

// Returns the two bit format used for the given number of bias bytes.
uint8_t BiasFormat(uint8_t bias_bytes) {
  switch (bias_bytes) {
    case 2:
      return two_byte_bias;
    case 3:
      return three_byte_bias;
    case 0:
    default:
      return no_bias;
  }
}

Status EncodeEntry(const PatchMap::Entry& entry, uint32_t last_entry_index,
                   PatchEncoding default_encoding, std::string& out) {
  const auto& coverage = entry.coverage;
  bool has_codepoints = !coverage.codepoints.empty();
  bool has_features = !coverage.features.empty();
  bool has_design_space = !coverage.design_space.empty();
  bool has_features_or_design_space = has_features || has_design_space;
  int64_t delta =
      ((int64_t)entry.patch_index) - ((int64_t)last_entry_index + 1);
  bool has_delta = delta != 0;
  bool has_patch_encoding = entry.encoding != default_encoding;

  uint8_t bias_bytes = BiasBytes(entry.coverage);

  // format
  uint8_t format =
      (has_features_or_design_space ? features_and_design_space_bit_mask
                                    : 0) |  // bit 0
      // not set, has copy mapping indices (bit 1) // bit 1
      (has_delta ? index_delta_bit_mask : 0) |        // bit 2
      (has_patch_encoding ? encoding_bit_mask : 0) |  // bit 3
      (has_codepoints ? codepoint_bit_mask & BiasFormat(bias_bytes)
                      : 0);  // bit 4 and 5
  // not set, ignore (bit 6)
  FontHelper::WriteUInt8(format, out);

  if (has_features_or_design_space) {
    WRITE_UINT8(coverage.features.size(), out,
                "Exceed max number of feature tags (0xFF).");
    for (hb_tag_t tag : coverage.features) {
      FontHelper::WriteUInt32(tag, out);
    }

    WRITE_UINT16(coverage.design_space.size(), out,
                 "Too many design space segments.");
    for (const auto& [tag, range] : coverage.design_space) {
      auto s = EncodeAxisSegment(tag, range, out);
      if (!s.ok()) {
        return s;
      }
    }
  }

  if (has_delta) {
    WRITE_INT24(delta, out,
                StrCat("Exceed max entry index delta (int24): ", delta));
  }

  if (has_patch_encoding) {
    auto encoding_value = EncodingToInt(entry.encoding);
    if (!encoding_value.ok()) {
      return encoding_value.status();
    }
    FontHelper::WriteUInt8(*encoding_value, out);
  }

  if (has_codepoints) {
    EncodeCodepoints(bias_bytes, entry.coverage, out);
  }

  return absl::OkStatus();
}

}  // namespace ift::proto
