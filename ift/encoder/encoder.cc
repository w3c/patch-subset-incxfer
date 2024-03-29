#include "ift/encoder/encoder.h"

#include <algorithm>
#include <iterator>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/axis_range.h"
#include "common/binary_diff.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/woff2.h"
#include "hb-subset.h"
#include "ift/encoder/iftb_patch_creator.h"
#include "ift/ift_client.h"
#include "ift/iftb_binary_patch.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::AxisRange;
using common::BinaryDiff;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::hb_set_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using common::make_hb_set;
using common::Woff2;
using ift::IftbBinaryPatch;
using ift::proto::IFT;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;

namespace ift::encoder {

void PrintTo(const Encoder::SubsetDefinition& def, std::ostream* os) {
  *os << "[{";

  btree_set<uint32_t> sorted;
  for (uint32_t cp : def.codepoints) {
    sorted.insert(cp);
  }

  bool first = true;
  for (uint32_t cp : sorted) {
    if (!first) {
      *os << ", ";
    }
    first = false;
    *os << cp;
  }

  *os << "}";

  if (!def.design_space.empty()) {
    *os << ", {";
    bool first = true;
    for (const auto& [tag, range] : def.design_space) {
      if (!first) {
        *os << ", ";
      }
      first = false;
      *os << FontHelper::ToString(tag) << ": ";
      PrintTo(range, os);
    }
    *os << "}";
  }

  *os << "]";
}

StatusOr<std::string> UrlTemplateFor(const FontData& font) {
  auto ift = IFTTable::FromFont(font);
  if (!ift.ok()) {
    return ift.status();
  }

  return ift->GetUrlTemplate();
}

void Encoder::AddCombinations(const std::vector<const SubsetDefinition*>& in,
                              uint32_t choose,
                              std::vector<Encoder::SubsetDefinition>& out) {
  if (!choose || in.size() < choose) {
    return;
  }

  if (choose == 1) {
    for (auto item : in) {
      out.push_back(*item);
    }
    return;
  }

  for (auto it = in.begin(); it != in.end(); it++) {
    auto it_inner = it + 1;
    std::vector<const SubsetDefinition*> remaining;
    std::copy(it_inner, in.end(), std::back_inserter(remaining));

    std::vector<Encoder::SubsetDefinition> combinations;
    AddCombinations(remaining, choose - 1, combinations);
    for (auto& s : combinations) {
      s.Union(**it);
      out.push_back(std::move(s));
    }
  }
}

std::vector<Encoder::SubsetDefinition> Encoder::OutgoingEdges(
    const SubsetDefinition& base_subset, uint32_t choose) const {
  std::vector<SubsetDefinition> remaining_subsets;
  for (const auto& s : extension_subsets_) {
    SubsetDefinition filtered = s;
    filtered.Subtract(base_subset);
    if (filtered.empty()) {
      continue;
    }

    remaining_subsets.push_back(std::move(filtered));
  }

  std::vector<const SubsetDefinition*> input;
  for (const auto& s : remaining_subsets) {
    input.push_back(&s);
  }

  std::vector<Encoder::SubsetDefinition> result;
  for (uint32_t i = 1; i <= choose; i++) {
    AddCombinations(input, i, result);
  }

  return result;
}

absl::flat_hash_set<uint32_t> subtract(const absl::flat_hash_set<uint32_t>& a,
                                       const absl::flat_hash_set<uint32_t>& b) {
  absl::flat_hash_set<uint32_t> c;
  for (uint32_t v : a) {
    if (!b.contains(v)) {
      c.insert(v);
    }
  }
  return c;
}

Encoder::design_space_t subtract(const Encoder::design_space_t& a,
                                 const Encoder::design_space_t& b) {
  Encoder::design_space_t c;

  for (const auto& [tag, range] : a) {
    auto e = b.find(tag);
    if (e == b.end()) {
      c[tag] = range;
      continue;
    }

    if (e->second.IsPoint()) {
      // range minus a point, does nothing.
      c[tag] = range;
    }

    // TODO(garretrieger): this currently operates only at the axis
    //  level. Partial ranges within an axis are not supported.
    //  to implement this we'll need to subtract the two ranges
    //  from each other. However, this can produce two resulting ranges
    //  instead of one.
    //
    //  It's likely that we'll forbid disjoint ranges, so we can simply
    //  error out if a configuration would result in one.
  }

  return c;
}

void Encoder::SubsetDefinition::Subtract(const SubsetDefinition& other) {
  codepoints = subtract(codepoints, other.codepoints);
  gids = subtract(gids, other.gids);
  feature_tags = subtract(feature_tags, other.feature_tags);
  design_space = subtract(design_space, other.design_space);
}

void Encoder::SubsetDefinition::Union(const SubsetDefinition& other) {
  std::copy(other.codepoints.begin(), other.codepoints.end(),
            std::inserter(codepoints, codepoints.begin()));
  std::copy(other.gids.begin(), other.gids.end(),
            std::inserter(gids, gids.begin()));
  std::copy(other.feature_tags.begin(), other.feature_tags.end(),
            std::inserter(feature_tags, feature_tags.begin()));

  for (const auto& [tag, range] : other.design_space) {
    auto existing = design_space.find(tag);
    if (existing == design_space.end()) {
      design_space[tag] = range;
      continue;
    }

    // TODO(garretrieger): this is a simplified implementation that
    //  only allows expanding a point to a range. This needs to be
    //  updated to handle a generic union.
    //
    //  It's likely that we'll forbid disjoint ranges, so we can simply
    //  error out if a configuration would result in one.
    if (existing->second.IsPoint() && range.IsRange()) {
      design_space[tag] = range;
    }
  }
}

void Encoder::SubsetDefinition::ConfigureInput(hb_subset_input_t* input,
                                               hb_face_t* face) const {
  hb_set_t* unicodes = hb_subset_input_unicode_set(input);
  for (hb_codepoint_t cp : codepoints) {
    hb_set_add(unicodes, cp);
  }

  hb_set_t* features =
      hb_subset_input_set(input, HB_SUBSET_SETS_LAYOUT_FEATURE_TAG);
  for (hb_tag_t tag : feature_tags) {
    hb_set_add(features, tag);
  }

  for (const auto& [tag, range] : design_space) {
    hb_subset_input_set_axis_range(input, face, tag, range.start(), range.end(),
                                   NAN);
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

PatchMap::Coverage Encoder::SubsetDefinition::ToCoverage() const {
  PatchMap::Coverage coverage;
  coverage.codepoints = codepoints;
  coverage.features = feature_tags;
  for (const auto& [tag, range] : design_space) {
    coverage.design_space[tag] = range;
  }
  return coverage;
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
  design_space_t empty;
  return SetBaseSubsetFromIftbPatches(included_patches, empty);
}

Status Encoder::SetBaseSubsetFromIftbPatches(
    const flat_hash_set<uint32_t>& included_patches,
    const design_space_t& design_space) {
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

  base_subset_.design_space = design_space;

  // remove all patches that have been placed into the base subset.
  RemoveIftbPatches(included_patches);

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

void Encoder::AddOptionalDesignSpace(const design_space_t& space) {
  SubsetDefinition def;
  def.design_space = space;
  extension_subsets_.push_back(def);
}

StatusOr<Encoder::SubsetDefinition> Encoder::SubsetDefinitionForIftbPatches(
    const flat_hash_set<uint32_t>& ids) const {
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

  auto sc = PopulateIftbPatches(base_subset_.design_space, UrlTemplate());
  for (const auto& [ds, url_template] : iftb_url_overrides_) {
    sc.Update(PopulateIftbPatches(ds, url_template));
  }

  if (!sc.ok()) {
    return sc;
  }

  return Encode(base_subset_, true);
}

Status Encoder::PopulateIftbPatches(const design_space_t& design_space,
                                    std::string url_template) {
  if (existing_iftb_patches_.empty()) {
    return absl::OkStatus();
  }

  FontData instance;
  instance.set(face_);

  if (!design_space.empty()) {
    // If a design space is provided, apply it.
    auto result = Instance(face_, design_space);
    if (!result.ok()) {
      return result.status();
    }
    instance.shallow_copy(*result);
  }

  for (const auto& e : existing_iftb_patches_) {
    uint32_t index = e.first;
    std::string url = IFTClient::PatchToUrl(url_template, index);

    SubsetDefinition subset = e.second;
    auto patch =
        IftbPatchCreator::CreatePatch(instance, index, Id(), subset.gids);
    if (!patch.ok()) {
      return patch.status();
    }

    patches_[url].shallow_copy(*patch);
  }

  return absl::OkStatus();
}

Status Encoder::PopulateIftbPatchMap(PatchMap& patch_map,
                                     const design_space_t& design_space) const {
  if (existing_iftb_patches_.empty()) {
    return absl::OkStatus();
  }

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
                                   bool is_root) {
  auto it = built_subsets_.find(base_subset);
  if (it != built_subsets_.end()) {
    FontData copy;
    copy.shallow_copy(it->second);
    return copy;
  }

  std::vector<SubsetDefinition> subsets =
      OutgoingEdges(base_subset, jump_ahead_);

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

  IFTTable table;
  auto url_template = iftb_url_overrides_.find(base_subset.design_space);
  bool replace_url_template =
      IsMixedMode() && (url_template != iftb_url_overrides_.end());
  if (!replace_url_template) {
    table.SetUrlTemplate(UrlTemplate());
  } else {
    // There's a different url template to use for the iftb entries
    // (which are stored in the main table).
    const std::string& iftb_url_template = url_template->second;
    table.SetUrlTemplate(iftb_url_template);
    table.SetExtensionUrlTemplate(UrlTemplate());
  }

  auto sc = table.SetId(Id());
  if (!sc.ok()) {
    return sc;
  }

  PatchMap& patch_map = table.GetPatchMap();
  sc = PopulateIftbPatchMap(patch_map, base_subset.design_space);
  if (!sc.ok()) {
    return sc;
  }

  std::vector<uint32_t> ids;
  bool as_extensions = IsMixedMode();
  PatchEncoding encoding =
      IsMixedMode() ? PER_TABLE_SHARED_BROTLI_ENCODING : SHARED_BROTLI_ENCODING;
  for (const auto& s : subsets) {
    uint32_t id = next_id_++;
    ids.push_back(id);

    PatchMap::Coverage coverage = s.ToCoverage();
    patch_map.AddEntry(coverage, id, encoding, as_extensions);
  }

  hb_face_t* face = base->reference_face();
  auto new_base = table.AddToFont(face, true);
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

  uint32_t i = 0;
  for (const auto& s : subsets) {
    uint32_t id = ids[i++];
    SubsetDefinition combined_subset = Combine(base_subset, s);
    auto next = Encode(combined_subset, false);
    if (!next.ok()) {
      return next.status();
    }

    FontData patch;
    auto differ = GetDifferFor(table, *next);
    if (!differ.ok()) {
      return differ.status();
    }
    Status sc = (*differ)->Diff(*base, *next, &patch);
    if (!sc.ok()) {
      return sc;
    }

    std::string url = IFTClient::PatchToUrl(UrlTemplate(), id);
    patches_[url].shallow_copy(patch);
  }

  return base;
}

StatusOr<const BinaryDiff*> Encoder::GetDifferFor(
    const IFTTable& base_table, const FontData& font_data) const {
  if (!IsMixedMode()) {
    return &binary_diff_;
  }

  auto new_url_template = UrlTemplateFor(font_data);
  if (!new_url_template.ok()) {
    return new_url_template.status();
  }

  if (base_table.GetUrlTemplate() != *new_url_template) {
    return &replace_ift_map_binary_diff_;
  }

  return &per_table_binary_diff_;
}

StatusOr<hb_face_unique_ptr> Encoder::CutSubsetFaceBuilder(
    hb_face_t* font, const SubsetDefinition& def) const {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  if (!input) {
    return absl::InternalError("Failed to create subset input.");
  }

  def.ConfigureInput(input, face_);

  SetIftbSubsettingFlagsIfNeeded(input);

  hb_face_unique_ptr result = make_hb_face(hb_subset_or_fail(font, input));
  if (!result.get()) {
    return absl::InternalError("Harfbuzz subsetting operation failed.");
  }

  hb_subset_input_destroy(input);
  return result;
}

StatusOr<FontData> Encoder::GenerateBaseGvar(
    hb_face_t* font, const design_space_t& design_space) const {
  // When generating a gvar table for use with IFTB patches care
  // must be taken to ensure that the shared tuples in the gvar
  // header match the shared tuples used in the per glyph data
  // in the previously created (via PopulateIftbPatches()) iftb
  // patches. However, we also want the gvar table to only contain
  // the glyphs from base_subset_. If you ran a single subsetting
  // operation through hb which reduced the glyphs and instanced
  // the design space the set of shared tuples may change.
  //
  // To keep the shared tuples correct we subset in two steps:
  // 1. Run instancing only, keeping everything else, this matches
  //    the processing done in PopulateIftbPatches() and will
  //    result in the same shared tuples.
  // 2. Run the glyph base subset, with no instancing specified.
  //    if there is no specified instancing then harfbuzz will
  //    not modify shared tuples.

  // Step 1: Instancing
  auto instance = Instance(font, design_space);
  if (!instance.ok()) {
    return instance.status();
  }

  // Step 2: glyph subsetting
  SubsetDefinition subset = base_subset_;
  // We don't want to apply any instancing here as it was done in step 1
  // so clear out the design space.
  subset.design_space = {};

  hb_face_unique_ptr instanced_face = instance->face();
  auto face_builder = CutSubsetFaceBuilder(instanced_face.get(), subset);
  if (!face_builder.ok()) {
    return face_builder.status();
  }

  // Step 3: extract gvar table.
  hb_blob_unique_ptr gvar_blob = make_hb_blob(
      hb_face_reference_table(face_builder->get(), HB_TAG('g', 'v', 'a', 'r')));
  FontData result(gvar_blob.get());
  return result;
}

void Encoder::SetIftbSubsettingFlagsIfNeeded(hb_subset_input_t* input) const {
  if (IsMixedMode()) {
    // Mixed mode requires stable gids and IFTB requirements to be met,
    // set flags accordingly.
    hb_subset_input_set_flags(
        input, hb_subset_input_get_flags(input) | HB_SUBSET_FLAGS_RETAIN_GIDS |
                   HB_SUBSET_FLAGS_IFTB_REQUIREMENTS |
                   HB_SUBSET_FLAGS_NOTDEF_OUTLINE |
                   HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
  }
}

StatusOr<FontData> Encoder::CutSubset(hb_face_t* font,
                                      const SubsetDefinition& def) const {
  auto result = CutSubsetFaceBuilder(font, def);
  if (!result.ok()) {
    return result.status();
  }

  if (IsMixedMode() && def.IsVariable()) {
    // In mixed mode iftb patches handles gvar, except for when design space
    // is expanded, in which case a gvar table should be patched in that only
    // has coverage of the base (root) subset definition + the current design
    // space.
    //
    // Create such a gvar table here and overwrite the one that was otherwise
    // generated by the normal subsetting operation. The patch generation will
    // handle including a replacement gvar patch when needed.
    auto base_gvar = GenerateBaseGvar(font, def.design_space);
    if (!base_gvar.ok()) {
      return base_gvar.status();
    }

    hb_blob_unique_ptr gvar_blob = base_gvar->blob();
    hb_face_builder_add_table(result->get(), HB_TAG('g', 'v', 'a', 'r'),
                              gvar_blob.get());
  }

  FontHelper::ApplyIftbTableOrdering(result->get());
  hb_blob_unique_ptr blob = make_hb_blob(hb_face_reference_blob(result->get()));

  FontData subset(blob.get());
  return subset;
}

StatusOr<FontData> Encoder::Instance(hb_face_t* face,
                                     const design_space_t& design_space) const {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();

  // Keep everything in this subset, except for applying the design space.
  hb_subset_input_keep_everything(input);
  SetIftbSubsettingFlagsIfNeeded(input);

  for (const auto& [tag, range] : design_space) {
    hb_subset_input_set_axis_range(input, face, tag, range.start(), range.end(),
                                   NAN);
  }

  hb_face_unique_ptr subset = make_hb_face(hb_subset_or_fail(face, input));
  hb_subset_input_destroy(input);

  if (!subset.get()) {
    return absl::InternalError("Instancing failed.");
  }

  FontHelper::ApplyIftbTableOrdering(subset.get());
  hb_blob_unique_ptr out = make_hb_blob(hb_face_reference_blob(subset.get()));

  FontData result(out.get());
  return result;
}

template <typename T>
void Encoder::RemoveIftbPatches(T ids) {
  for (uint32_t id : ids) {
    existing_iftb_patches_.erase(id);
  }
}

StatusOr<FontData> Encoder::RoundTripWoff2(string_view font,
                                           bool glyf_transform) {
  auto r = Woff2::EncodeWoff2(font, glyf_transform);
  if (!r.ok()) {
    return r.status();
  }

  return Woff2::DecodeWoff2(r->str());
}

}  // namespace ift::encoder
