#include "ift/encoder/encoder.h"

#include <algorithm>
#include <iterator>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/binary_diff.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "hb-subset.h"
#include "ift/iftb_binary_patch.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"
#include "woff2/decode.h"
#include "woff2/encode.h"
#include "woff2/output.h"

using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::BinaryDiff;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using ift::IftbBinaryPatch;
using ift::proto::IFT;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;
using woff2::ComputeWOFF2FinalSize;
using woff2::ConvertTTFToWOFF2;
using woff2::ConvertWOFF2ToTTF;
using woff2::MaxWOFF2CompressedSize;
using woff2::WOFF2Params;
using woff2::WOFF2StringOut;

namespace ift::encoder {

std::vector<const Encoder::SubsetDefinition*> Encoder::Remaining(
    const std::vector<const SubsetDefinition*>& subsets,
    const SubsetDefinition* subset) const {
  std::vector<const SubsetDefinition*> remaining_subsets;
  std::copy_if(subsets.begin(), subsets.end(),
               std::back_inserter(remaining_subsets),
               [&](const SubsetDefinition* s) { return s != subset; });
  return remaining_subsets;
}

void Encoder::SubsetDefinition::Union(const SubsetDefinition& other) {
  std::copy(other.codepoints.begin(), other.codepoints.end(),
            std::inserter(codepoints, codepoints.begin()));
  std::copy(other.gids.begin(), other.gids.end(),
            std::inserter(gids, gids.begin()));
  std::copy(other.feature_tags.begin(), other.feature_tags.end(),
            std::inserter(feature_tags, feature_tags.begin()));
}

void Encoder::SubsetDefinition::ConfigureInput(hb_subset_input_t* input) const {
  hb_set_t* unicodes = hb_subset_input_unicode_set(input);
  for (hb_codepoint_t cp : codepoints) {
    hb_set_add(unicodes, cp);
  }

  hb_set_t* features =
      hb_subset_input_set(input, HB_SUBSET_SETS_LAYOUT_FEATURE_TAG);
  for (hb_tag_t tag : feature_tags) {
    hb_set_add(features, tag);
  }

  if (gids.empty()) {
    return;
  }

  hb_set_t* gids_set = hb_subset_input_glyph_set(input);
  hb_set_add(gids_set, 0);
  for (hb_codepoint_t gid : gids) {
    hb_set_add(gids_set, gid);
  }
}

Encoder::SubsetDefinition Encoder::Combine(const SubsetDefinition& s1,
                                           const SubsetDefinition& s2) const {
  SubsetDefinition result;
  result.Union(s1);
  result.Union(s2);
  return result;
}

Status Encoder::AddExistingIftbPatch(uint32_t id, const FontData& patch) {
  if (!face_) {
    return absl::FailedPreconditionError("Encoder must have a face set.");
  }

  auto gids = IftbBinaryPatch::GidsInPatch(patch);
  if (!gids.ok()) {
    return gids.status();
  }

  uint32_t glyph_count = hb_face_get_glyph_count(face_);

  SubsetDefinition subset;
  auto gid_to_unicode = FontHelper::GidToUnicodeMap(face_);
  for (uint32_t gid : *gids) {
    subset.gids.insert(gid);

    auto cp = gid_to_unicode.find(gid);
    if (cp == gid_to_unicode.end()) {
      if (gid >= glyph_count) {
        return absl::InvalidArgumentError(
            StrCat("Patch has gid, ", gid, ", which is not in the font."));
      }
      // Gid is in the font but not in the cmap, ignore it.
      continue;
    }

    subset.codepoints.insert(cp->second);
  }

  existing_iftb_patches_[id] = subset;
  next_id_ = std::max(next_id_, id + 1);
  return absl::OkStatus();
}

Status Encoder::AddIftbFeatureSpecificPatch(uint32_t original_id, uint32_t id,
                                            hb_tag_t feature_tag) {
  if (!existing_iftb_patches_.contains(original_id)) {
    return absl::InvalidArgumentError(
        StrCat("IFTB patch ", original_id,
               " has not been supplied via AddExistingIftbPatch()"));
  }
  if (!existing_iftb_patches_.contains(id)) {
    return absl::InvalidArgumentError(
        StrCat("IFTB patch ", id,
               " has not been supplied via AddExistingIftbPatch()"));
  }

  iftb_feature_mappings_[id][feature_tag].insert(original_id);
  return absl::OkStatus();
}

Status Encoder::SetBaseSubsetFromIftbPatches(
    const flat_hash_set<uint32_t>& included_patches) {
  // TODO(garretrieger): handle the case where a patch included in the
  //  base subset has associated feature specific patches. We could
  //  merge those in as well, or create special entries for them that only
  //  utilize feature tag to trigger.
  if (!face_) {
    return absl::FailedPreconditionError("Encoder must have a face set.");
  }

  if (!base_subset_.empty()) {
    return absl::FailedPreconditionError("Base subset has already been set.");
  }

  for (uint32_t id : included_patches) {
    if (!existing_iftb_patches_.contains(id)) {
      return absl::InvalidArgumentError(
          StrCat("IFTB patch, ", id, ", not added to the encoder."));
    }
  }

  flat_hash_set<uint32_t> excluded_patches;
  for (const auto& p : existing_iftb_patches_) {
    if (!included_patches.contains(p.first)) {
      excluded_patches.insert(p.first);
    }
  }

  auto excluded = SubsetDefinitionForIftbPatches(excluded_patches);
  if (!excluded.ok()) {
    return excluded.status();
  }

  uint32_t glyph_count = hb_face_get_glyph_count(face_);
  for (uint32_t gid = 0; gid < glyph_count; gid++) {
    if (!excluded->gids.contains(gid)) {
      base_subset_.gids.insert(gid);
    }
  }

  hb_set_unique_ptr cps_in_font = make_hb_set();
  hb_face_collect_unicodes(face_, cps_in_font.get());
  uint32_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(cps_in_font.get(), &cp)) {
    if (!excluded->codepoints.contains(cp)) {
      base_subset_.codepoints.insert(cp);
    }
  }

  for (uint32_t id : included_patches) {
    // remove all patches that have been placed into the base subset.
    existing_iftb_patches_.erase(id);
  }

  // TODO(garretrieger):
  //   This is a hack, the IFTB merger does not support loca len changing.
  //   so always include the last gid in the base subset to force the
  //   loca table to remain at the full length from the start. This should
  //   be removed once that limitation is fixed in the IFTB merger.
  uint32_t gid_count = hb_face_get_glyph_count(face_);
  if (gid_count > 0) base_subset_.gids.insert(gid_count - 1);

  return absl::OkStatus();
}

Status Encoder::AddExtensionSubsetOfIftbPatches(
    const flat_hash_set<uint32_t>& ids) {
  auto subset = SubsetDefinitionForIftbPatches(ids);
  if (!subset.ok()) {
    return subset.status();
  }

  extension_subsets_.push_back(*subset);
  return absl::OkStatus();
}

void Encoder::AddOptionalFeatureGroup(
    const flat_hash_set<hb_tag_t>& feature_tags) {
  SubsetDefinition def;
  def.feature_tags = feature_tags;
  extension_subsets_.push_back(def);
}

StatusOr<Encoder::SubsetDefinition> Encoder::SubsetDefinitionForIftbPatches(
    const flat_hash_set<uint32_t>& ids) {
  SubsetDefinition result;
  for (uint32_t id : ids) {
    auto p = existing_iftb_patches_.find(id);
    if (p == existing_iftb_patches_.end()) {
      return absl::InvalidArgumentError(
          StrCat("IFTB patch id, ", id, ", not found."));
    }
    result.Union(p->second);
  }
  return result;
}

StatusOr<FontData> Encoder::Encode() {
  if (!face_) {
    return absl::FailedPreconditionError("Encoder must have a face set.");
  }

  std::vector<const SubsetDefinition*> subsets;
  for (const auto& s : extension_subsets_) {
    subsets.push_back(&s);
  }

  return Encode(base_subset_, subsets, true);
}

Status Encoder::PopulateIftbPatchMap(PatchMap& patch_map) {
  for (const auto& e : existing_iftb_patches_) {
    uint32_t id = e.first;
    auto it = iftb_feature_mappings_.find(id);
    if (it == iftb_feature_mappings_.end()) {
      // Just a regular entry mapped by codepoints only.
      patch_map.AddEntry(e.second.codepoints, e.first, IFTB_ENCODING);
      continue;
    }

    // this is a feature specific entry and so uses the subset definition from
    // another patch + a feature tag.
    for (const auto& [feature_tag, indices] : it->second) {
      PatchMap::Coverage coverage;
      coverage.features.insert(feature_tag);

      for (uint32_t original_id : indices) {
        auto original = existing_iftb_patches_.find(original_id);
        if (original == existing_iftb_patches_.end()) {
          return absl::InvalidArgumentError(
              StrCat("Original iftb patch ", original_id, " not found."));
        }
        const auto& original_def = original->second;

        // TODO(garretrieger): optimize the patch map and use "subset indices"
        //  instead of respecifying the codepoint subset.
        std::copy(
            original_def.codepoints.begin(), original_def.codepoints.end(),
            std::inserter(coverage.codepoints, coverage.codepoints.begin()));
      }

      patch_map.AddEntry(coverage, id, IFTB_ENCODING);
    }
  }
  return absl::OkStatus();
}

StatusOr<FontData> Encoder::Encode(const SubsetDefinition& base_subset,
                                   std::vector<const SubsetDefinition*> subsets,
                                   bool is_root) {
  auto it = built_subsets_.find(base_subset);
  if (it != built_subsets_.end()) {
    FontData copy;
    copy.shallow_copy(it->second);
    return copy;
  }

  // TODO(garretrieger): when subsets are overlapping modify subsets at each
  // iteration to remove
  //                     codepoints which are in the base at this step.

  // The first subset forms the base file, the remaining subsets are made
  // reachable via patches.
  auto base = CutSubset(face_, base_subset);
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
  auto sc = PopulateIftbPatchMap(patch_map);
  if (!sc.ok()) {
    return sc;
  }

  std::vector<uint32_t> ids;
  bool as_extensions = IsMixedMode();
  PatchEncoding encoding =
      IsMixedMode() ? PER_TABLE_SHARED_BROTLI_ENCODING : SHARED_BROTLI_ENCODING;
  for (auto s : subsets) {
    uint32_t id = next_id_++;
    ids.push_back(id);

    PatchMap::Coverage coverage;
    coverage.codepoints = s->codepoints;
    coverage.features = s->feature_tags;
    patch_map.AddEntry(coverage, id, encoding, as_extensions);
  }

  patch_map.AddToProto(ift_proto);
  if (IsMixedMode()) {
    patch_map.AddToProto(iftx_proto, true);
  }

  hb_face_t* face = base->reference_face();
  auto new_base = IFTTable::AddToFont(
      face, ift_proto, IsMixedMode() ? &iftx_proto : nullptr, true);
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
    std::vector<const SubsetDefinition*> remaining_subsets =
        Remaining(subsets, s);
    SubsetDefinition combined_subset = Combine(base_subset, *s);
    auto next = Encode(combined_subset, remaining_subsets, false);
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

StatusOr<FontData> Encoder::CutSubset(hb_face_t* font,
                                      const SubsetDefinition& def) {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  if (!input) {
    return absl::InternalError("Failed to create subset input.");
  }

  def.ConfigureInput(input);

  if (IsMixedMode()) {
    // Mixed mode requires stable gids and IFTB requirements to be met,
    // set flags accordingly.
    hb_subset_input_set_flags(
        input, HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_IFTB_REQUIREMENTS |
                   HB_SUBSET_FLAGS_NOTDEF_OUTLINE |
                   HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
  }

  hb_face_t* result = hb_subset_or_fail(font, input);
  FontHelper::ApplyIftbTableOrdering(result);
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
