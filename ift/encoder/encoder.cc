#include "ift/encoder/encoder.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/axis_range.h"
#include "common/binary_diff.h"
#include "common/compat_id.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/try.h"
#include "common/woff2.h"
#include "hb-subset.h"
#include "ift/glyph_keyed_diff.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_encoding.h"
#include "ift/proto/patch_map.h"
#include "ift/url_template.h"

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::AxisRange;
using common::BinaryDiff;
using common::CompatId;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::hb_set_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using common::make_hb_set;
using common::Woff2;
using ift::GlyphKeyedDiff;
using ift::proto::GLYPH_KEYED;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::TABLE_KEYED_FULL;
using ift::proto::TABLE_KEYED_PARTIAL;

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

StatusOr<FontData> Encoder::FullyExpandedSubset(
    const ProcessingContext& context) const {
  SubsetDefinition all;
  all.Union(base_subset_);
  for (const auto& s : extension_subsets_) {
    all.Union(s);
  }
  for (const auto& [id, s] : glyph_data_segments_) {
    all.Union(s);
  }

  // Union doesn't work completely correctly with respect to design spaces so
  // clear out design space which will just include the full original design
  // space.
  // TODO(garretrieger): once union works correctly remove this.
  all.design_space.clear();

  return CutSubset(context, face_.get(), all);
}

bool is_subset(const flat_hash_set<uint32_t>& a,
               const flat_hash_set<uint32_t>& b) {
  return std::all_of(b.begin(), b.end(),
                     [&a](const uint32_t& v) { return a.count(v) > 0; });
}

Encoder::SubsetDefinition Encoder::AddFeatureSpecificChunksIfNeeded(
    const SubsetDefinition& def) const {
  // When using optional feature segments we may encounter a particular subset
  // during the table keyed analysis whose subset definition contains the
  // necessary prerequisites to trigger the inclusion of a some feature specific
  // glyph keyed patches. If this is the case then the subset def must be
  // expanded to include any glyphs from those patches since the IFT font can
  // access those glyphs at this level of extension and inclusion of the extra
  // glyphs may affect things like loca size.
  SubsetDefinition out;
  out.Union(def);
  /*
  // TODO XXXX reimplement under the new conditions framework.
  for (hb_tag_t feature : def.feature_tags) {
    // for each included feature check if for any glyph keyed patches that could
    // be activated under def and add them to the subset.
    for (const auto& [added_segment_id, triggers] :
         glyph_data_segment_feature_dependencies_) {
      auto added_segment = glyph_data_segments_.find(added_segment_id);
      if (added_segment == glyph_data_segments_.end()) {
        continue;
      }

      for (const auto& [trigger_feature, trigger_segments] : triggers) {
        if (trigger_feature != feature) {
          continue;
        }

        for (uint32_t trigger_id : trigger_segments) {
          auto segment = glyph_data_segments_.find(trigger_id);
          if (segment == glyph_data_segments_.end()) {
            continue;
          }

          if (!is_subset(def.gids, segment->second.gids)) {
            continue;
          }

          // The appropriate feature and glyphs are present in def to trigger
          // the inclusion of added_segment_id so union it into the subset def.
          out.Union(added_segment->second);
        }
      }
    }
  }
  */
  return out;
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

template <typename S>
S subtract(const S& a, const S& b) {
  S c;
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

  return AddFeatureSpecificChunksIfNeeded(result);
}

Status Encoder::AddGlyphDataSegment(uint32_t id,
                                    const absl::flat_hash_set<uint32_t>& gids) {
  if (!face_) {
    return absl::FailedPreconditionError("Encoder must have a face set.");
  }

  if (glyph_data_segments_.contains(id)) {
    return absl::FailedPreconditionError(
        StrCat("A segment with id, ", id, ", has already been supplied."));
  }

  uint32_t glyph_count = hb_face_get_glyph_count(face_.get());

  SubsetDefinition subset;
  auto gid_to_unicode = FontHelper::GidToUnicodeMap(face_.get());
  for (uint32_t gid : gids) {
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

  glyph_data_segments_[id] = subset;
  next_id_ = std::max(next_id_, id + 1);
  return absl::OkStatus();
}

Status Encoder::AddGlyphDataActivationCondition(Condition condition) {
  /*
  TODO check all referenced ids exist.
  if (!glyph_data_segments_.contains(original_id)) {
    return absl::InvalidArgumentError(
        StrCat("Glyh keyed segment ", original_id,
               " has not been supplied via AddGlyphDataSegment()"));
  }
  if (!glyph_data_segments_.contains(id)) {
    return absl::InvalidArgumentError(
        StrCat("Glyph keyed segment ", id,
               " has not been supplied via AddGlyphDataSegment()"));
  }
  */
  return absl::UnimplementedError("TODO");
}

Status Encoder::AddFeatureDependency(uint32_t original_id, uint32_t id,
                                     hb_tag_t feature_tag) {
  Condition condition;
  condition.activated_segment_id = id;
  condition.required_features.insert(feature_tag);
  condition.required_groups.push_back({original_id});

  return AddGlyphDataActivationCondition(std::move(condition));
}

Status Encoder::SetBaseSubsetFromSegments(
    const flat_hash_set<uint32_t>& included_segments) {
  design_space_t empty;
  return SetBaseSubsetFromSegments(included_segments, empty);
}

Status Encoder::SetBaseSubsetFromSegments(
    const flat_hash_set<uint32_t>& included_segments,
    const design_space_t& design_space) {
  // TODO(garretrieger): support also providing initial features.
  // TODO(garretrieger): resolve dependencies that are satisified by the
  // included patches, features and design space
  //                     and pull those into the base subset.
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

  for (uint32_t id : included_segments) {
    if (!glyph_data_segments_.contains(id)) {
      return absl::InvalidArgumentError(
          StrCat("Glyph data segment, ", id, ", not added to the encoder."));
    }
  }

  flat_hash_set<uint32_t> excluded_segments;
  for (const auto& p : glyph_data_segments_) {
    if (!included_segments.contains(p.first)) {
      excluded_segments.insert(p.first);
    }
  }

  auto excluded = SubsetDefinitionForSegments(excluded_segments);
  if (!excluded.ok()) {
    return excluded.status();
  }

  uint32_t glyph_count = hb_face_get_glyph_count(face_.get());
  for (uint32_t gid = 0; gid < glyph_count; gid++) {
    if (!excluded->gids.contains(gid)) {
      base_subset_.gids.insert(gid);
    }
  }

  hb_set_unique_ptr cps_in_font = make_hb_set();
  hb_face_collect_unicodes(face_.get(), cps_in_font.get());
  uint32_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(cps_in_font.get(), &cp)) {
    if (!excluded->codepoints.contains(cp)) {
      base_subset_.codepoints.insert(cp);
    }
  }

  base_subset_.design_space = design_space;

  // remove all segments that have been placed into the base subset.
  RemoveSegments(included_segments);

  // Glyph keyed patches can't change the glyph count in the font (and hence
  // loca len) so always include the last gid in the base subset to force the
  // loca table to remain at the full length from the start.
  //
  // TODO(garretrieger): this unnecessarily includes the last gid in the subset,
  // should
  //                     update the subsetter to retain the glyph count but not
  //                     actually keep the last gid.
  //
  // TODO(garretrieger): instead of forcing max glyph count here we can utilize
  // table keyed patches
  //                     to change loca len/glyph count to the max for any
  //                     currently reachable segments. This would improve
  //                     efficiency slightly by avoid including extra space in
  //                     the initial font.
  uint32_t gid_count = hb_face_get_glyph_count(face_.get());
  if (gid_count > 0) base_subset_.gids.insert(gid_count - 1);

  return absl::OkStatus();
}

Status Encoder::AddNonGlyphSegmentFromGlyphSegments(
    const flat_hash_set<uint32_t>& ids) {
  auto subset = SubsetDefinitionForSegments(ids);
  if (!subset.ok()) {
    return subset.status();
  }

  extension_subsets_.push_back(*subset);
  return absl::OkStatus();
}

void Encoder::AddFeatureGroupSegment(const btree_set<hb_tag_t>& feature_tags) {
  SubsetDefinition def;
  def.feature_tags = feature_tags;
  extension_subsets_.push_back(def);
}

void Encoder::AddDesignSpaceSegment(const design_space_t& space) {
  SubsetDefinition def;
  def.design_space = space;
  extension_subsets_.push_back(def);
}

StatusOr<Encoder::SubsetDefinition> Encoder::SubsetDefinitionForSegments(
    const flat_hash_set<uint32_t>& ids) const {
  SubsetDefinition result;
  for (uint32_t id : ids) {
    auto p = glyph_data_segments_.find(id);
    if (p == glyph_data_segments_.end()) {
      return absl::InvalidArgumentError(
          StrCat("Glyph data segment, ", id, ", not found."));
    }
    result.Union(p->second);
  }
  return result;
}

StatusOr<Encoder::Encoding> Encoder::Encode() const {
  if (!face_) {
    return absl::FailedPreconditionError("Encoder must have a face set.");
  }

  ProcessingContext context(next_id_);
  context.force_long_loca_and_gvar_ = false;
  auto expanded = FullyExpandedSubset(context);
  if (!expanded.ok()) {
    return expanded.status();
  }

  context.fully_expanded_subset_.shallow_copy(*expanded);
  auto expanded_face = expanded->face();
  context.force_long_loca_and_gvar_ =
      FontHelper::HasLongLoca(expanded_face.get()) ||
      FontHelper::HasWideGvar(expanded_face.get());

  auto init_font = Encode(context, base_subset_, true);
  if (!init_font.ok()) {
    return init_font.status();
  }

  Encoding result;
  result.init_font.shallow_copy(*init_font);
  result.patches = std::move(context.patches_);
  return result;
}

bool Encoder::AllocatePatchSet(ProcessingContext& context,
                               const design_space_t& design_space,
                               std::string& uri_template,
                               CompatId& compat_id) const {
  auto uri_it = context.patch_set_uri_templates_.find(design_space);
  auto compat_id_it = context.glyph_keyed_compat_ids_.find(design_space);

  bool has_uri = (uri_it != context.patch_set_uri_templates_.end());
  bool has_compat_id = (compat_id_it != context.glyph_keyed_compat_ids_.end());

  if (has_uri && has_compat_id) {
    // already created, return existing.
    uri_template = uri_it->second;
    compat_id = compat_id_it->second;
    return false;
  }

  uri_template = UrlTemplate(context.next_patch_set_id_++);
  compat_id = context.GenerateCompatId();

  context.patch_set_uri_templates_[design_space] = uri_template;
  context.glyph_keyed_compat_ids_[design_space] = compat_id;
  return true;
}

Status Encoder::EnsureGlyphKeyedPatchesPopulated(
    ProcessingContext& context, const design_space_t& design_space,
    std::string& uri_template, CompatId& compat_id) const {
  if (glyph_data_segments_.empty()) {
    return absl::OkStatus();
  }

  if (!AllocatePatchSet(context, design_space, uri_template, compat_id)) {
    // Patches have already been populated for this design space.
    return absl::OkStatus();
  }

  auto full_face = context.fully_expanded_subset_.face();
  FontData instance;
  instance.set(full_face.get());

  if (!design_space.empty()) {
    // If a design space is provided, apply it.
    auto result = Instance(context, full_face.get(), design_space);
    if (!result.ok()) {
      return result.status();
    }
    instance.shallow_copy(*result);
  }

  GlyphKeyedDiff differ(instance, compat_id,
                        {FontHelper::kGlyf, FontHelper::kGvar});

  for (const auto& e : glyph_data_segments_) {
    uint32_t index = e.first;

    std::string url = URLTemplate::PatchToUrl(uri_template, index);

    SubsetDefinition subset = e.second;
    btree_set<uint32_t> gids;
    std::copy(subset.gids.begin(), subset.gids.end(),
              std::inserter(gids, gids.begin()));
    auto patch = differ.CreatePatch(gids);
    if (!patch.ok()) {
      return patch.status();
    }

    context.patches_[url].shallow_copy(*patch);
  }

  return absl::OkStatus();
}

void PrintTo(const Encoder::Condition& c, std::ostream* os) {
  *os << "{";
  for (const auto& group : c.required_groups) {
    *os << "{";
    for (auto v : group) {
      *os << v << ", ";
    }
    *os << "}, ";
  }

  *os << "}, {";

  for (auto t : c.required_features) {
    *os << FontHelper::ToString(t) << ", ";
  }

  *os << "} => " << c.activated_segment_id;
}

bool Encoder::Condition::operator<(const Condition& other) const {
  if (required_groups.size() != other.required_groups.size()) {
    return required_groups.size() < other.required_groups.size();
  }

  auto a = required_groups.begin();
  auto b = other.required_groups.begin();
  while (a != required_groups.end() && b != required_groups.end()) {
    if (a->size() != b->size()) {
      return a->size() < b->size();
    }

    auto aa = a->begin();
    auto bb = b->begin();
    while (aa != a->end() && bb != b->end()) {
      if (*aa != *bb) {
        return *aa < *bb;
      }
      aa++;
      bb++;
    }

    a++;
    b++;
  }

  if (required_features.size() != other.required_features.size()) {
    return required_features.size() < other.required_features.size();
  }

  auto f_a = required_features.begin();
  auto f_b = other.required_features.begin();
  while (f_a != required_features.end() &&
         f_b != other.required_features.end()) {
    if (*f_a != *f_b) {
      return *f_a < *f_b;
    }
    f_a++;
    f_b++;
  }

  if (activated_segment_id != other.activated_segment_id) {
    return activated_segment_id < other.activated_segment_id;
  }

  // These two are equal
  return false;
}

Status Encoder::PopulateGlyphKeyedPatchMap(PatchMap& patch_map) const {
  if (glyph_data_segments_.empty()) {
    return absl::OkStatus();
  }

  // TODO XXXXX handle features.

  // The conditions list describes what the patch map should do, here
  // we need to convert that into an equivalent list of entries.
  //
  // To minimize encoded size we can reuse set definitions in later entries
  // via the copy indices mechanism. The conditions are evaluated in three
  // phases to successively build up a set of common entries which can be reused
  // by later ones.

  // Tracks the list of conditions which have not yet been placed in a map
  // entry.
  btree_set<Condition> remaining_conditions = activation_conditions_;

  // Phase 1 generate the base entries, there should be one for each
  // unique glyph segment that is referenced in at least one condition.
  // the conditions will refer back to these base entries via copy indices
  //
  // Each base entry can be used to map one condition as well.
  flat_hash_map<uint32_t, uint32_t> patch_id_to_entry_index;
  uint32_t next_entry_index = 0;
  uint32_t last_patch_id = 0;
  for (auto condition = remaining_conditions.begin();
       condition != remaining_conditions.begin();) {
    bool remove = false;
    for (const auto& group : condition->required_groups) {
      for (uint32_t patch_id : group) {
        if (patch_id_to_entry_index.contains(patch_id)) {
          continue;
        }

        auto original = glyph_data_segments_.find(patch_id);
        if (original == glyph_data_segments_.end()) {
          return absl::InvalidArgumentError(
              StrCat("Glyph data patch ", patch_id, " not found."));
        }
        const auto& original_def = original->second;

        PatchMap::Coverage coverage;
        coverage.codepoints = original_def.codepoints;

        if (condition->IsUnitary()) {
          // this condition can use this entry to map itself.
          TRYV(patch_map.AddEntry(coverage, condition->activated_segment_id,
                                  GLYPH_KEYED));
          last_patch_id = condition->activated_segment_id;
          remove = true;
        } else {
          // Otherwise this entry does nothing (ignored = true), but will be
          // referenced by later entries the assigned id doesn't matter, but
          // using last_patch_id + 1 it will avoid needing to encoding the entry
          // id delta.
          TRYV(
              patch_map.AddEntry(coverage, ++last_patch_id, GLYPH_KEYED, true));
        }

        patch_id_to_entry_index[patch_id] = next_entry_index++;
      }
    }

    if (remove) {
      remaining_conditions.erase(condition++);
    } else {
      ++condition;
    }
  }

  // Phase 2 generate entries for all groups of patches reusing the base entries
  // written in phase one. When writing an entry if the triggering group is the
  // only one in the condition then that condition can utilize the entry (just
  // like in Phase 1).
  flat_hash_map<btree_set<uint32_t>, uint32_t> patch_group_to_entry_index;
  for (auto condition = remaining_conditions.begin();
       condition != remaining_conditions.begin();) {
    bool remove = false;
    for (const auto& group : condition->required_groups) {
      if (group.size() <= 1 || patch_group_to_entry_index.contains(group)) {
        // don't handle groups of size one, those will just reference the base
        // entry directly.
        continue;
      }

      PatchMap::Coverage coverage;
      coverage.copy_mode_append =
          false;  // union the group members together (OR).
      coverage.copy_indices = group;

      if (condition->required_groups.size() == 1) {
        TRYV(patch_map.AddEntry(coverage, condition->activated_segment_id,
                                GLYPH_KEYED));
        last_patch_id = condition->activated_segment_id;
        remove = true;
      } else {
        TRYV(patch_map.AddEntry(coverage, ++last_patch_id, GLYPH_KEYED, true));
      }

      patch_group_to_entry_index[group] = next_entry_index++;
    }

    if (remove) {
      remaining_conditions.erase(condition++);
    } else {
      ++condition;
    }
  }

  // Phase 3 for any remaining conditions create the actual entries utilizing
  // the groups (phase 2) and base entries (phase 1) as needed
  for (auto condition = remaining_conditions.begin();
       condition != remaining_conditions.begin(); condition++) {
    PatchMap::Coverage coverage;
    coverage.copy_mode_append = true;  // append the groups (AND)

    for (const auto& group : condition->required_groups) {
      if (group.size() == 1) {
        coverage.copy_indices.insert(patch_id_to_entry_index[*group.begin()]);
        continue;
      }

      coverage.copy_indices.insert(patch_group_to_entry_index[group]);
    }

    TRYV(patch_map.AddEntry(coverage, condition->activated_segment_id,
                            GLYPH_KEYED));
  }

  return absl::OkStatus();
}

StatusOr<FontData> Encoder::Encode(ProcessingContext& context,
                                   const SubsetDefinition& base_subset,
                                   bool is_root) const {
  auto it = context.built_subsets_.find(base_subset);
  if (it != context.built_subsets_.end()) {
    FontData copy;
    copy.shallow_copy(it->second);
    return copy;
  }

  std::string table_keyed_uri_template = UrlTemplate(0);
  CompatId table_keyed_compat_id = context.GenerateCompatId();
  std::string glyph_keyed_uri_template;
  CompatId glyph_keyed_compat_id;
  auto sc = EnsureGlyphKeyedPatchesPopulated(context, base_subset.design_space,
                                             glyph_keyed_uri_template,
                                             glyph_keyed_compat_id);
  if (!sc.ok()) {
    return sc;
  }

  std::vector<SubsetDefinition> subsets =
      OutgoingEdges(base_subset, jump_ahead_);

  // The first subset forms the base file, the remaining subsets are made
  // reachable via patches.
  auto full_face = context.fully_expanded_subset_.face();
  auto base = CutSubset(context, full_face.get(), base_subset);
  if (!base.ok()) {
    return base.status();
  }

  if (subsets.empty() && !IsMixedMode()) {
    // This is a leaf node, a IFT table isn't needed.
    context.built_subsets_[base_subset].shallow_copy(*base);
    return base;
  }

  IFTTable table_keyed;
  IFTTable glyph_keyed;
  table_keyed.SetId(table_keyed_compat_id);
  table_keyed.SetUrlTemplate(table_keyed_uri_template);
  glyph_keyed.SetId(glyph_keyed_compat_id);
  glyph_keyed.SetUrlTemplate(glyph_keyed_uri_template);

  PatchMap& glyph_keyed_patch_map = glyph_keyed.GetPatchMap();
  sc = PopulateGlyphKeyedPatchMap(glyph_keyed_patch_map);
  if (!sc.ok()) {
    return sc;
  }

  std::vector<uint32_t> ids;
  PatchMap& table_keyed_patch_map = table_keyed.GetPatchMap();
  PatchEncoding encoding =
      IsMixedMode() ? TABLE_KEYED_PARTIAL : TABLE_KEYED_FULL;
  for (const auto& s : subsets) {
    uint32_t id = context.next_id_++;
    ids.push_back(id);

    PatchMap::Coverage coverage = s.ToCoverage();
    TRYV(table_keyed_patch_map.AddEntry(coverage, id, encoding));
  }

  auto face = base->face();
  std::optional<IFTTable*> ext =
      IsMixedMode() ? std::optional(&glyph_keyed) : std::nullopt;
  auto new_base = IFTTable::AddToFont(face.get(), table_keyed, ext);

  if (!new_base.ok()) {
    return new_base.status();
  }

  if (is_root) {
    // For the root node round trip the font through woff2 so that the base for
    // patching can be a decoded woff2 font file.
    base = RoundTripWoff2(new_base->str(), false);
    if (!base.ok()) {
      return base.status();
    }
  } else {
    base->shallow_copy(*new_base);
  }

  context.built_subsets_[base_subset].shallow_copy(*base);

  uint32_t i = 0;
  for (const auto& s : subsets) {
    uint32_t id = ids[i++];
    SubsetDefinition combined_subset = Combine(base_subset, s);
    auto next = Encode(context, combined_subset, false);
    if (!next.ok()) {
      return next.status();
    }

    // Check if the main table URL will change with this subset
    std::string next_glyph_keyed_uri_template;
    CompatId next_glyph_keyed_compat_id;
    auto sc = EnsureGlyphKeyedPatchesPopulated(
        context, base_subset.design_space, glyph_keyed_uri_template,
        glyph_keyed_compat_id);
    if (!sc.ok()) {
      return sc;
    }

    bool replace_url_template =
        IsMixedMode() &&
        (next_glyph_keyed_uri_template != glyph_keyed_uri_template);

    FontData patch;
    auto differ =
        GetDifferFor(*next, table_keyed_compat_id, replace_url_template);
    if (!differ.ok()) {
      return differ.status();
    }
    sc = (*differ)->Diff(*base, *next, &patch);
    if (!sc.ok()) {
      return sc;
    }

    std::string url = URLTemplate::PatchToUrl(table_keyed_uri_template, id);
    context.patches_[url].shallow_copy(patch);
  }

  return base;
}

StatusOr<std::unique_ptr<const BinaryDiff>> Encoder::GetDifferFor(
    const FontData& font_data, CompatId compat_id,
    bool replace_url_template) const {
  if (!IsMixedMode()) {
    return std::unique_ptr<const BinaryDiff>(
        Encoder::FullFontTableKeyedDiff(compat_id));
  }

  if (replace_url_template) {
    return std::unique_ptr<const BinaryDiff>(
        Encoder::ReplaceIftMapTableKeyedDiff(compat_id));
  }

  return std::unique_ptr<const BinaryDiff>(
      Encoder::MixedModeTableKeyedDiff(compat_id));
}

StatusOr<hb_face_unique_ptr> Encoder::CutSubsetFaceBuilder(
    const ProcessingContext& context, hb_face_t* font,
    const SubsetDefinition& def) const {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  if (!input) {
    return absl::InternalError("Failed to create subset input.");
  }

  def.ConfigureInput(input, font);

  SetMixedModeSubsettingFlagsIfNeeded(context, input);

  hb_face_unique_ptr result = make_hb_face(hb_subset_or_fail(font, input));
  if (!result.get()) {
    return absl::InternalError("Harfbuzz subsetting operation failed.");
  }

  hb_subset_input_destroy(input);
  return result;
}

StatusOr<FontData> Encoder::GenerateBaseGvar(
    const ProcessingContext& context, hb_face_t* font,
    const design_space_t& design_space) const {
  // When generating a gvar table for use with glyph keyed patches care
  // must be taken to ensure that the shared tuples in the gvar
  // header match the shared tuples used in the per glyph data
  // in the previously created (via GlyphKeyedDiff) glyph keyed
  // patches. However, we also want the gvar table to only contain
  // the glyphs from base_subset_. If you ran a single subsetting
  // operation through hb which reduced the glyphs and instanced
  // the design space the set of shared tuples may change.
  //
  // To keep the shared tuples correct we subset in two steps:
  // 1. Run instancing only, keeping everything else, this matches
  //    the processing done in EnsureGlyphKeyedPatchesPopulated()
  //    and will result in the same shared tuples.
  // 2. Run the glyph base subset, with no instancing specified.
  //    if there is no specified instancing then harfbuzz will
  //    not modify shared tuples.

  // Step 1: Instancing
  auto instance = Instance(context, font, design_space);
  if (!instance.ok()) {
    return instance.status();
  }

  // Step 2: glyph subsetting
  SubsetDefinition subset = base_subset_;
  // We don't want to apply any instancing here as it was done in step 1
  // so clear out the design space.
  subset.design_space = {};

  hb_face_unique_ptr instanced_face = instance->face();
  auto face_builder =
      CutSubsetFaceBuilder(context, instanced_face.get(), subset);
  if (!face_builder.ok()) {
    return face_builder.status();
  }

  // Step 3: extract gvar table.
  hb_blob_unique_ptr gvar_blob = make_hb_blob(
      hb_face_reference_table(face_builder->get(), HB_TAG('g', 'v', 'a', 'r')));
  FontData result(gvar_blob.get());
  return result;
}

void Encoder::SetMixedModeSubsettingFlagsIfNeeded(
    const ProcessingContext& context, hb_subset_input_t* input) const {
  if (IsMixedMode()) {
    // Mixed mode requires stable gids set flags accordingly.
    hb_subset_input_set_flags(
        input, hb_subset_input_get_flags(input) | HB_SUBSET_FLAGS_RETAIN_GIDS |
                   HB_SUBSET_FLAGS_NOTDEF_OUTLINE |
                   HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);

    if (context.force_long_loca_and_gvar_) {
      // IFTB requirements flag has the side effect of forcing long loca and
      // gvar.
      hb_subset_input_set_flags(input, hb_subset_input_get_flags(input) |
                                           HB_SUBSET_FLAGS_IFTB_REQUIREMENTS);
    }
  }
}

StatusOr<FontData> Encoder::CutSubset(const ProcessingContext& context,
                                      hb_face_t* font,
                                      const SubsetDefinition& def) const {
  auto result = CutSubsetFaceBuilder(context, font, def);
  if (!result.ok()) {
    return result.status();
  }

  if (IsMixedMode() && def.IsVariable()) {
    // In mixed mode glyph keyed patches handles gvar, except for when design
    // space is expanded, in which case a gvar table should be patched in that
    // only has coverage of the base (root) subset definition + the current
    // design space.
    //
    // Create such a gvar table here and overwrite the one that was otherwise
    // generated by the normal subsetting operation. The patch generation will
    // handle including a replacement gvar patch when needed.
    auto base_gvar = GenerateBaseGvar(context, font, def.design_space);
    if (!base_gvar.ok()) {
      return base_gvar.status();
    }

    hb_blob_unique_ptr gvar_blob = base_gvar->blob();
    hb_face_builder_add_table(result->get(), HB_TAG('g', 'v', 'a', 'r'),
                              gvar_blob.get());
  }

  hb_blob_unique_ptr blob = make_hb_blob(hb_face_reference_blob(result->get()));

  FontData subset(blob.get());
  return subset;
}

StatusOr<FontData> Encoder::Instance(const ProcessingContext& context,
                                     hb_face_t* face,
                                     const design_space_t& design_space) const {
  hb_subset_input_t* input = hb_subset_input_create_or_fail();

  // Keep everything in this subset, except for applying the design space.
  hb_subset_input_keep_everything(input);
  SetMixedModeSubsettingFlagsIfNeeded(context, input);

  for (const auto& [tag, range] : design_space) {
    hb_subset_input_set_axis_range(input, face, tag, range.start(), range.end(),
                                   NAN);
  }

  hb_face_unique_ptr subset = make_hb_face(hb_subset_or_fail(face, input));
  hb_subset_input_destroy(input);

  if (!subset.get()) {
    return absl::InternalError("Instancing failed.");
  }

  hb_blob_unique_ptr out = make_hb_blob(hb_face_reference_blob(subset.get()));

  FontData result(out.get());
  return result;
}

template <typename T>
void Encoder::RemoveSegments(T ids) {
  for (uint32_t id : ids) {
    glyph_data_segments_.erase(id);
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

CompatId Encoder::ProcessingContext::GenerateCompatId() {
  return CompatId(
      this->random_values_(this->gen_), this->random_values_(this->gen_),
      this->random_values_(this->gen_), this->random_values_(this->gen_));
}

}  // namespace ift::encoder
