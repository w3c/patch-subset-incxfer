#include "ift/encoder/encoder.h"

#include <iterator>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "hb-subset.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"
#include "patch_subset/binary_diff.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "woff2/decode.h"
#include "woff2/encode.h"
#include "woff2/output.h"

using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::string_view;
using ift::proto::IFT;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BinaryDiff;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using woff2::ComputeWOFF2FinalSize;
using woff2::ConvertTTFToWOFF2;
using woff2::ConvertWOFF2ToTTF;
using woff2::MaxWOFF2CompressedSize;
using woff2::WOFF2Params;
using woff2::WOFF2StringOut;

namespace ift::encoder {

std::vector<const flat_hash_set<hb_codepoint_t>*> remaining(
    const std::vector<const flat_hash_set<hb_codepoint_t>*>& subsets,
    const flat_hash_set<hb_codepoint_t>* subset) {
  std::vector<const flat_hash_set<hb_codepoint_t>*> remaining_subsets;
  std::copy_if(
      subsets.begin(), subsets.end(), std::back_inserter(remaining_subsets),
      [&](const flat_hash_set<hb_codepoint_t>* s) { return s != subset; });
  return remaining_subsets;
}

flat_hash_set<hb_codepoint_t> combine(const flat_hash_set<hb_codepoint_t>& s1,
                                      const flat_hash_set<hb_codepoint_t>& s2) {
  flat_hash_set<hb_codepoint_t> result;
  std::copy(s1.begin(), s1.end(), std::inserter(result, result.begin()));
  std::copy(s2.begin(), s2.end(), std::inserter(result, result.begin()));
  return result;
}

StatusOr<FontData> Encoder::Encode(
    hb_face_t* font, const flat_hash_set<hb_codepoint_t>& base_subset,
    std::vector<const flat_hash_set<hb_codepoint_t>*> subsets, bool is_root) {
  auto it = built_subsets_.find(base_subset);
  if (it != built_subsets_.end()) {
    FontData copy;
    copy.shallow_copy(it->second);
    return copy;
  }

  // The first subset forms the base file, the remaining subsets are made
  // reachable via patches.
  auto base = CutSubset(font, base_subset);
  if (!base.ok()) {
    return base.status();
  }

  if (subsets.empty() && !IsMixedMode()) {
    // This is a leaf node, a IFT table isn't needed.
    built_subsets_[base_subset].shallow_copy(*base);
    return base;
  }

  IFT ift_proto, iftx_proto;
  ift_proto.set_url_template(UrlTemplate());
  if (!IsMixedMode()) {
    ift_proto.set_default_patch_encoding(SHARED_BROTLI_ENCODING);
  } else {
    ift_proto.set_default_patch_encoding(IFTB_ENCODING);
    iftx_proto.set_default_patch_encoding(PER_TABLE_SHARED_BROTLI_ENCODING);
  }
  for (uint32_t p : Id()) {
    ift_proto.add_id(p);
  }

  PatchMap patch_map;

  std::vector<uint32_t> ids;
  for (const auto& e : existing_iftb_patches_) {
    patch_map.AddEntry(e.second, e.first, IFTB_ENCODING);
  }

  bool as_extensions = IsMixedMode();
  PatchEncoding encoding =
      IsMixedMode() ? PER_TABLE_SHARED_BROTLI_ENCODING : SHARED_BROTLI_ENCODING;
  for (auto s : subsets) {
    uint32_t id = next_id_++;
    ids.push_back(id);

    PatchMap::Coverage coverage;
    coverage.codepoints = *s;
    patch_map.AddEntry(coverage, id, encoding, as_extensions);
  }

  patch_map.AddToProto(ift_proto);
  if (IsMixedMode()) {
    patch_map.AddToProto(iftx_proto, true);
  }

  hb_face_t* face = base->reference_face();
  auto new_base = IFTTable::AddToFont(face, ift_proto,
                                      IsMixedMode() ? &iftx_proto : nullptr);
  hb_face_destroy(face);

  if (!new_base.ok()) {
    return new_base.status();
  }

  if (is_root) {
    // For the root node round trip the font through woff2 so that the base for
    // patching can be a decoded woff2 font file.
    base = RoundTripWoff2(new_base->str(), false);
  } else {
    base->shallow_copy(*new_base);
  }

  built_subsets_[base_subset].shallow_copy(*base);
  const BinaryDiff* differ = IsMixedMode()
                                 ? (BinaryDiff*)&per_table_binary_diff_
                                 : (BinaryDiff*)&binary_diff_;

  uint32_t i = 0;
  for (auto s : subsets) {
    uint32_t id = ids[i++];
    std::vector<const flat_hash_set<hb_codepoint_t>*> remaining_subsets =
        remaining(subsets, s);
    flat_hash_set<hb_codepoint_t> combined_subset = combine(base_subset, *s);
    auto next = Encode(font, combined_subset, remaining_subsets, false);
    if (!next.ok()) {
      return next.status();
    }

    FontData patch;
    Status sc = differ->Diff(*base, *next, &patch);
    if (!sc.ok()) {
      return sc;
    }

    patches_[id].shallow_copy(patch);
  }

  return base;
}

StatusOr<FontData> Encoder::CutSubset(
    hb_face_t* font, const flat_hash_set<hb_codepoint_t>& codepoints) {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  if (!input) {
    return absl::InternalError("Failed to create subset input.");
  }

  hb_set_t* unicodes = hb_subset_input_unicode_set(input);
  for (hb_codepoint_t cp : codepoints) {
    hb_set_add(unicodes, cp);
  }

  if (IsMixedMode()) {
    // Mixed mode requires stable gids, so set retain gids.
    hb_subset_input_set_flags(input, HB_SUBSET_FLAGS_RETAIN_GIDS);
  }

  hb_face_t* result = hb_subset_or_fail(font, input);
  hb_blob_t* blob = hb_face_reference_blob(result);

  FontData subset(blob);

  hb_blob_destroy(blob);
  hb_face_destroy(result);
  hb_subset_input_destroy(input);

  return subset;
}

StatusOr<FontData> Encoder::EncodeWoff2(string_view font, bool glyf_transform) {
  WOFF2Params params;
  params.brotli_quality = 11;
  params.allow_transforms = glyf_transform;
  params.preserve_table_order =
      true;  // IFTB patches require a specific table ordering.
  size_t buffer_size =
      MaxWOFF2CompressedSize((const uint8_t*)font.data(), font.size());
  uint8_t* buffer = (uint8_t*)malloc(buffer_size);
  if (!ConvertTTFToWOFF2((const uint8_t*)font.data(), font.size(), buffer,
                         &buffer_size, params)) {
    free(buffer);
    return absl::InternalError("WOFF2 encoding failed.");
  }

  hb_blob_t* blob = hb_blob_create((const char*)buffer, buffer_size,
                                   HB_MEMORY_MODE_READONLY, buffer, free);
  FontData result(blob);
  hb_blob_destroy(blob);
  return result;
}

StatusOr<FontData> Encoder::DecodeWoff2(string_view font) {
  size_t buffer_size =
      ComputeWOFF2FinalSize((const uint8_t*)font.data(), font.size());
  if (!buffer_size) {
    return absl::InternalError("Failed computing woff2 output size.");
  }

  std::string buffer;
  buffer.resize(buffer_size);
  WOFF2StringOut out(&buffer);

  if (!ConvertWOFF2ToTTF((const uint8_t*)font.data(), font.size(), &out)) {
    return absl::InternalError("WOFF2 decoding failed.");
  }

  FontData result(buffer);
  return result;
}

StatusOr<FontData> Encoder::RoundTripWoff2(string_view font,
                                           bool glyf_transform) {
  auto r = EncodeWoff2(font, glyf_transform);
  if (!r.ok()) {
    return r.status();
  }

  return DecodeWoff2(r->str());
}

}  // namespace ift::encoder