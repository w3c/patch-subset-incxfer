#include "ift/proto/format_2_patch_map.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/patch_map.h"

using absl::Span;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;

#define WRITE_UINT8(V, O, M)                     \
  if (FontHelper::WillIntOverflow<uint8_t>(V)) { \
    return absl::InvalidArgumentError(M);        \
  }                                              \
  FontHelper::WriteUInt8(V, O);

#define WRITE_UINT16(V, O, M)                     \
  if (FontHelper::WillIntOverflow<uint16_t>(V)) { \
    return absl::InvalidArgumentError(M);         \
  }                                               \
  FontHelper::WriteUInt16(V, O);

#define WRITE_INT16(V, O, M)                      \
  if (FontHelper::WillIntOverflow<uint16_t>(V)) { \
    return absl::InvalidArgumentError(M);         \
  }                                               \
  FontHelper::WriteUInt16(V, O);

namespace ift::proto {

static StatusOr<uint8_t> EncodingToInt(PatchEncoding encoding) {
  if (encoding == IFTB_ENCODING) {
    return 0;
  }

  if (encoding == SHARED_BROTLI_ENCODING) {
    return 1;
  }

  if (encoding == PER_TABLE_SHARED_BROTLI_ENCODING) {
    return 2;
  }

  return absl::InvalidArgumentError(
      StrCat("Unknown patch encoding, ", encoding));
}

static PatchEncoding PickDefaultEncoding(const PatchMap& patch_map) {
  uint32_t counts[3] = {0, 0, 0};
  for (const auto& e : patch_map.GetEntries()) {
    auto i = EncodingToInt(e.encoding);
    if (!i.ok()) {
      // Ignore.
      continue;
    }
    counts[*i]++;
  }

  if (counts[0] >= counts[1] && counts[0] >= counts[2]) {
    return IFTB_ENCODING;
  }

  if (counts[1] >= counts[0] && counts[1] >= counts[2]) {
    return SHARED_BROTLI_ENCODING;
  }

  return PER_TABLE_SHARED_BROTLI_ENCODING;
}

static Status EncodeEntries(Span<const PatchMap::Entry> entries, bool is_ext,
                            PatchEncoding default_encoding, std::string& out);

static Status EncodeEntry(const PatchMap::Entry& entry,
                          uint32_t last_entry_index,
                          PatchEncoding default_encoding, std::string& out);

Status Format2PatchMap::Deserialize(absl::string_view data, PatchMap& out) {
  // TODO
  return absl::UnimplementedError("Not implemented yet.");
}

StatusOr<std::string> Format2PatchMap::Serialize(const PatchMap& patch_map,
                                                 bool is_ext,
                                                 string_view uri_template) {
  // TODO(garretrieger): prereserve estimated capacity based on patch_map.
  std::string out;

  FontHelper::WriteUInt8(0x02, out);  // Format = 2
  FontHelper::WriteUInt32(0x0, out);  // Reserved = 0x00000000

  // TODO(garretrieger): actually add the Id.
  FontHelper::WriteUInt32(0x0, out);  // Id[0] = 0x00000000
  FontHelper::WriteUInt32(0x0, out);  // Id[1] = 0x00000000
  FontHelper::WriteUInt32(0x0, out);  // Id[2] = 0x00000000
  FontHelper::WriteUInt32(0x0, out);  // Id[3] = 0x00000000

  // defaultPatchEncoding
  PatchEncoding default_encoding = PickDefaultEncoding(patch_map);
  auto encoding_value = EncodingToInt(default_encoding);
  if (!encoding_value.ok()) {
    return encoding_value.status();
  }
  FontHelper::WriteUInt8(*encoding_value, out);

  // mappingCount
  WRITE_UINT16(patch_map.GetEntries().size(), out,
               "Exceeded maximum number of entries (0xFFFF).");

  // mappings
  constexpr int header_min_length = 22;
  FontHelper::WriteUInt32(header_min_length + uri_template.length(), out);

  // idStrings
  FontHelper::WriteUInt32(0, out);

  // uriTemplateLength
  WRITE_UINT16(uri_template.length(), out,
               "Exceeded maximum uri template size (0xFFFF)");

  // uriTemplate
  out.append(uri_template);

  return EncodeEntries(patch_map.GetEntries(), is_ext, default_encoding, out);
}

Status EncodeEntries(Span<const PatchMap::Entry> entries, bool is_ext,
                     PatchEncoding default_encoding, std::string& out) {
  // TODO(garretrieger): identify and copy existing entries when possible.
  uint32_t last_entry_index = 0;
  for (const auto& entry : entries) {
    if (entry.extension_entry != is_ext) {
      continue;
    }

    auto s = EncodeEntry(entry, last_entry_index, default_encoding, out);
    if (!s.ok()) {
      return s;
    }
    last_entry_index = entry.patch_index;
  }

  return absl::OkStatus();
}

Status EncodeEntry(const PatchMap::Entry& entry, uint32_t last_entry_index,
                   PatchEncoding default_encoding, std::string& out) {
  const auto& coverage = entry.coverage;
  bool has_codepoints = !coverage.codepoints.empty();
  bool has_features = !coverage.features.empty();
  bool has_design_space = !coverage.design_space.empty();
  int64_t delta = ((int64_t)entry.patch_index) - ((int64_t)last_entry_index);
  bool has_delta = delta != 1;
  bool has_patch_encoding = entry.encoding != default_encoding;

  // format
  uint8_t format = (has_features ? 1 : 0) | (has_design_space ? 1 << 1 : 0) |
                   // not set, has copy mapping indices (bit 2)
                   (has_delta ? 1 << 3 : 0) |
                   (has_patch_encoding ? 1 << 4 : 0) |
                   (has_codepoints ? 1 << 5 : 0);
  // not set, ignore (bit 6)
  FontHelper::WriteUInt8(format, out);

  if (has_features) {
    WRITE_UINT8(coverage.features.size(), out,
                "Exceed max number of feature tags (0xFF).");
    for (hb_tag_t tag : coverage.features) {
      FontHelper::WriteUInt32(tag, out);
    }
  }

  if (has_design_space) {
    // TODO(garretrieger): implement me.
    return absl::UnimplementedError("Not implemented yet.");
  }

  if (has_delta) {
    WRITE_INT16(delta, out, "Exceed max entry index delta (int16).");
  }

  if (has_patch_encoding) {
    auto encoding_value = EncodingToInt(entry.encoding);
    if (!encoding_value.ok()) {
      return encoding_value.status();
    }
    FontHelper::WriteUInt8(*encoding_value, out);
  }

  if (has_codepoints) {
    uint32_t bias = coverage.SmallestCodepoint();
    hb_set_unique_ptr biased_set = make_hb_set();
    for (uint32_t cp : coverage.codepoints) {
      hb_set_add(biased_set.get(), cp - bias);
    }

    std::string sparse_bit_set = SparseBitSet::Encode(*biased_set);
    out.append(sparse_bit_set);
  }

  return absl::OkStatus();
}

}  // namespace ift::proto