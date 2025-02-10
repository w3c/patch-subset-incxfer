#include "ift/encoder/glyph_segmentation.h"

#include <cstdint>
#include <cstdio>
#include <sstream>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "common/compat_id.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/try.h"
#include "hb-subset.h"
#include "ift/glyph_keyed_diff.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::CompatId;
using common::FontData;
using common::FontHelper;
using common::hb_face_unique_ptr;
using common::hb_set_unique_ptr;
using common::make_hb_face;
using common::make_hb_set;

namespace ift::encoder {

// TODO(garretrieger): extensions/improvements that could be made:
// - Multi segment combination testing with GSUB dep analysis to guide.
// - Use merging and/or duplication to ensure minimum patch size.
//   - base patches (IN PROGRESS):
//   - composite patches (NOT STARTED)
// - Add logging

btree_set<uint32_t> to_btree_set(const hb_set_t* set) {
  btree_set<uint32_t> out;
  uint32_t v = HB_SET_VALUE_INVALID;
  while (hb_set_next(set, &v)) {
    out.insert(v);
  }
  return out;
}

void subtract(btree_set<uint32_t>& a, const btree_set<uint32_t>& b) {
  for (auto v : b) {
    a.erase(v);
  }
}

class GlyphConditions {
 public:
  GlyphConditions() : and_segments(make_hb_set()), or_segments(make_hb_set()) {}
  hb_set_unique_ptr and_segments;
  hb_set_unique_ptr or_segments;

  void RemoveSegments(const hb_set_t* segments) {
    hb_set_subtract(and_segments.get(), segments);
    hb_set_subtract(or_segments.get(), segments);
  }
};

class SegmentationContext;

Status AnalyzeSegment(SegmentationContext& context, const hb_set_t* codepoints,
                      hb_set_t* and_gids, hb_set_t* or_gids,
                      hb_set_t* exclusive_gids);

class SegmentationContext {
 public:
  SegmentationContext(
      hb_face_t* face, const flat_hash_set<uint32_t>& initial_segment,
      const std::vector<flat_hash_set<uint32_t>>& codepoint_segments)
      : preprocessed_face(make_hb_face(hb_subset_preprocess(face))),
        original_face(make_hb_face(hb_face_reference(face))),
        segments(),
        initial_codepoints(make_hb_set(initial_segment)),
        all_codepoints(make_hb_set()),
        full_closure(make_hb_set()),
        initial_closure(make_hb_set()) {
    for (const auto& s : codepoint_segments) {
      segments.push_back(make_hb_set(s));
    }

    hb_set_union(all_codepoints.get(), initial_codepoints.get());
    for (const auto& s : segments) {
      hb_set_union(all_codepoints.get(), s.get());
    }

    {
      auto closure = glyph_closure(initial_codepoints.get());
      if (closure.ok()) {
        initial_closure.reset(closure->release());
      }
    }

    auto closure = glyph_closure(all_codepoints.get());
    if (closure.ok()) {
      full_closure.reset(closure->release());
    }

    gid_conditions.resize(hb_face_get_glyph_count(original_face.get()));
  }

  void ResetGroupings() {
    unmapped_glyphs = {};
    and_glyph_groups = {};
    or_glyph_groups = {};
    patch_id_to_segment_index = {};
    fallback_segments = {};
  }

  StatusOr<hb_set_unique_ptr> glyph_closure(const hb_set_t* codepoints) {
    hb_subset_input_t* input = hb_subset_input_create_or_fail();
    if (!input) {
      return absl::InternalError("Closure subset configuration failed.");
    }

    hb_set_union(hb_subset_input_unicode_set(input), codepoints);
    // TODO(garretrieger): configure features (and other settings) appropriately
    // based on the IFT default feature list.

    hb_subset_plan_t* plan =
        hb_subset_plan_create_or_fail(preprocessed_face.get(), input);
    hb_subset_input_destroy(input);
    if (!plan) {
      return absl::InternalError("Closure calculation failed.");
    }

    hb_map_t* new_to_old = hb_subset_plan_new_to_old_glyph_mapping(plan);
    hb_set_unique_ptr gids = common::make_hb_set();
    hb_map_values(new_to_old, gids.get());
    hb_subset_plan_destroy(plan);

    return gids;
  }

  StatusOr<const hb_set_t*> code_point_set_to_or_gids(
      const hb_set_unique_ptr& codepoints) {
    auto hash_set_codepoints = common::to_hash_set(codepoints);

    auto it = code_point_set_to_or_gids_cache.find(hash_set_codepoints);
    if (it != code_point_set_to_or_gids_cache.end()) {
      return it->second.get();
    }

    hb_set_unique_ptr and_gids = make_hb_set();
    hb_set_unique_ptr or_gids = make_hb_set();
    hb_set_unique_ptr exclusive_gids = make_hb_set();
    TRYV(AnalyzeSegment(*this, codepoints.get(), and_gids.get(), or_gids.get(),
                        exclusive_gids.get()));

    const hb_set_t* or_gids_ptr = or_gids.get();
    code_point_set_to_or_gids_cache.insert(
        std::pair(hash_set_codepoints, std::move(or_gids)));
    return or_gids_ptr;
  }

  // Init
  common::hb_face_unique_ptr preprocessed_face;
  common::hb_face_unique_ptr original_face;
  std::vector<hb_set_unique_ptr> segments;

  hb_set_unique_ptr initial_codepoints;
  hb_set_unique_ptr all_codepoints;
  hb_set_unique_ptr full_closure;
  hb_set_unique_ptr initial_closure;

  // Phase 1
  std::vector<GlyphConditions> gid_conditions;

  // Phase 2
  btree_set<glyph_id_t> unmapped_glyphs;
  btree_map<btree_set<segment_index_t>, btree_set<glyph_id_t>> and_glyph_groups;
  btree_map<btree_set<segment_index_t>, btree_set<glyph_id_t>> or_glyph_groups;
  std::vector<segment_index_t> patch_id_to_segment_index;
  btree_set<segment_index_t> fallback_segments;
  flat_hash_map<flat_hash_set<uint32_t>, hb_set_unique_ptr>
      code_point_set_to_or_gids_cache;
};

Status AnalyzeSegment(SegmentationContext& context, const hb_set_t* codepoints,
                      hb_set_t* and_gids, hb_set_t* or_gids,
                      hb_set_t* exclusive_gids) {
  if (hb_set_is_empty(codepoints)) {
    // Skip empty sets, they will never contribute any conditions.
    return absl::OkStatus();
  }

  // This function tests various closures using the segment codepoints to
  // determine what conditions are present for the inclusion of closure glyphs.
  //
  // At a high level we do the following (where s_i is the segment being
  // tested):
  //
  // * Set A: glyph closure on original font of the union of all segments.
  // * Set B: glyph closure on original font of the union of all segments except
  //          for s_i
  // * Set I: (glyph closure on original font of s_0 union s_i) - (glyph closure
  //           on original font of s_0)
  // * Set D: A - B, the set of glyphs that are dropped when s_i is removed.
  //
  // Then we know the following:
  // * Glyphs in I should be included whenever s_i is activated.
  // * s_i is necessary for glyphs in D to be required, but other segments may
  //   be needed too.
  //
  // Furthermore we can intersect I and D to produce three sets:
  // * D - I: the activation condition for these glyphs is s_i AND …
  //          Where … is one or more additional segments.
  // * I - D: the activation conditions for these glyphs is s_i OR …
  //          Where … is one or more additional segments.
  // * D intersection I: the activation conditions for these glyphs is only s_i
  hb_set_unique_ptr except_segment = make_hb_set();
  hb_set_union(except_segment.get(), context.all_codepoints.get());
  hb_set_subtract(except_segment.get(), codepoints);
  auto B_except_segment_closure =
      TRY(context.glyph_closure(except_segment.get()));

  hb_set_unique_ptr only_segment = make_hb_set();
  hb_set_union(only_segment.get(), context.initial_codepoints.get());
  hb_set_union(only_segment.get(), codepoints);
  auto I_only_segment_closure = TRY(context.glyph_closure(only_segment.get()));
  hb_set_subtract(I_only_segment_closure.get(), context.initial_closure.get());

  hb_set_unique_ptr D_dropped = make_hb_set();
  hb_set_union(D_dropped.get(), context.full_closure.get());
  hb_set_subtract(D_dropped.get(), B_except_segment_closure.get());

  hb_set_union(and_gids, D_dropped.get());
  hb_set_subtract(and_gids, I_only_segment_closure.get());

  hb_set_union(or_gids, I_only_segment_closure.get());
  hb_set_subtract(or_gids, D_dropped.get());

  hb_set_union(exclusive_gids, I_only_segment_closure.get());
  hb_set_intersect(exclusive_gids, D_dropped.get());

  return absl::OkStatus();
}

Status AnalyzeSegment(SegmentationContext& context,
                      segment_index_t segment_index,
                      const hb_set_t* codepoints) {
  hb_set_unique_ptr and_gids = make_hb_set();
  hb_set_unique_ptr or_gids = make_hb_set();
  hb_set_unique_ptr exclusive_gids = make_hb_set();
  TRYV(AnalyzeSegment(context, codepoints, and_gids.get(), or_gids.get(),
                      exclusive_gids.get()));

  hb_codepoint_t and_gid = HB_SET_VALUE_INVALID;
  while (hb_set_next(exclusive_gids.get(), &and_gid)) {
    // TODO(garretrieger): if we are assigning an exclusive gid there should be
    // no other and segments, check and error if this is violated.
    hb_set_add(context.gid_conditions[and_gid].and_segments.get(),
               segment_index);
  }
  while (hb_set_next(and_gids.get(), &and_gid)) {
    hb_set_add(context.gid_conditions[and_gid].and_segments.get(),
               segment_index);
  }

  hb_codepoint_t or_gid = HB_SET_VALUE_INVALID;
  while (hb_set_next(or_gids.get(), &or_gid)) {
    hb_set_add(context.gid_conditions[or_gid].or_segments.get(), segment_index);
  }

  return absl::OkStatus();
}

Status GroupGlyphs(SegmentationContext& context) {
  btree_set<segment_index_t> fallback_segments_set;
  for (segment_index_t s = 0; s < context.segments.size(); s++) {
    if (hb_set_is_empty(context.segments[s].get())) {
      // Ignore empty segments.
      continue;
    }
    fallback_segments_set.insert(s);
  }

  for (glyph_id_t gid = 0; gid < context.gid_conditions.size(); gid++) {
    const auto& condition = context.gid_conditions[gid];
    if (!hb_set_is_empty(condition.and_segments.get())) {
      auto set = to_btree_set(condition.and_segments.get());
      context.and_glyph_groups[set].insert(gid);
    }
    if (!hb_set_is_empty(condition.or_segments.get())) {
      auto set = to_btree_set(condition.or_segments.get());
      context.or_glyph_groups[set].insert(gid);
    }

    if (hb_set_is_empty(condition.and_segments.get()) &&
        hb_set_is_empty(condition.or_segments.get()) &&
        !hb_set_has(context.initial_closure.get(), gid) &&
        hb_set_has(context.full_closure.get(), gid)) {
      context.unmapped_glyphs.insert(gid);
    }
  }

  // Any of the or_set conditions we've generated may have some additional
  // conditions that were not detected. Therefore we need to rule out the
  // presence of these additional conditions if an or group is able to be used.
  for (auto& [or_group, glyphs] : context.or_glyph_groups) {
    hb_set_unique_ptr all_other_codepoints = make_hb_set();
    hb_set_union(all_other_codepoints.get(), context.all_codepoints.get());
    for (uint32_t s : or_group) {
      hb_set_subtract(all_other_codepoints.get(), context.segments[s].get());
    }

    const hb_set_t* or_gids =
        TRY(context.code_point_set_to_or_gids(all_other_codepoints));

    // Any "OR" glyphs associated with all other codepoints have some additional
    // conditions to activate so we can't safely include them into this or
    // condition. They are instead moved to the set of unmapped glyphs.
    uint32_t gid = HB_SET_VALUE_INVALID;
    while (hb_set_next(or_gids, &gid)) {
      if (glyphs.erase(gid) > 0) {
        context.unmapped_glyphs.insert(gid);
      }
    }
  }

  for (uint32_t gid : context.unmapped_glyphs) {
    // this glyph is not activated anywhere but is needed in the full closure
    // so add it to an activation condition of any segment.
    context.or_glyph_groups[fallback_segments_set].insert(gid);
  }

  context.fallback_segments = std::move(fallback_segments_set);

  return absl::OkStatus();
}

Status GlyphSegmentation::GroupsToSegmentation(
    const btree_map<btree_set<segment_index_t>, btree_set<glyph_id_t>>&
        and_glyph_groups,
    const btree_map<btree_set<segment_index_t>, btree_set<glyph_id_t>>&
        or_glyph_groups,
    const btree_set<segment_index_t>& fallback_group,
    std::vector<segment_index_t>& patch_id_to_segment_index,
    GlyphSegmentation& segmentation) {
  patch_id_t next_id = 0;
  std::vector<patch_id_t> segment_to_patch_id;

  // Map segments into patch ids
  for (const auto& [and_segments, glyphs] : and_glyph_groups) {
    if (and_segments.size() != 1) {
      continue;
    }

    segment_index_t segment = *and_segments.begin();
    segmentation.patches_.insert(std::pair(next_id, glyphs));
    segmentation.conditions_.insert(
        ActivationCondition::and_patches({next_id}, next_id));

    if (segment + 1 > segment_to_patch_id.size()) {
      uint32_t size = segment_to_patch_id.size();
      for (uint32_t i = 0; i < (segment + 1) - size; i++) {
        segment_to_patch_id.push_back(-1);
      }
    }

    patch_id_to_segment_index.push_back(segment);
    segment_to_patch_id[segment] = next_id++;
  }

  for (const auto& [and_segments, glyphs] : and_glyph_groups) {
    if (and_segments.size() == 1) {
      // already processed above
      continue;
    }

    btree_set<patch_id_t> and_patches;
    for (segment_index_t segment : and_segments) {
      if (segment_to_patch_id[segment] == -1) {
        return absl::InternalError(StrCat(
            "Segment s", segment,
            " does not have an assigned patch id (found in an and_segment)."));
      }
      and_patches.insert(segment_to_patch_id[segment]);
    }

    segmentation.patches_.insert(std::pair(next_id, glyphs));
    segmentation.conditions_.insert(
        ActivationCondition::and_patches(and_patches, next_id));

    next_id++;
  }

  for (const auto& [or_segments, glyphs] : or_glyph_groups) {
    if (glyphs.empty()) {
      // Some or_segments have all of their glyphs removed by the additional
      // conditions check, don't create a patch for these.
      continue;
    }

    if (or_segments.size() == 1) {
      return absl::InternalError(
          StrCat("Unexpected or_segment with only one segment: s",
                 *or_segments.begin()));
    }
    btree_set<patch_id_t> or_patches;
    for (segment_index_t segment : or_segments) {
      if (segment_to_patch_id[segment] == -1) {
        return absl::InternalError(StrCat(
            "Segment s", segment,
            " does not have an assigned patch id (found in an or_segment)."));
      }

      if (!or_patches.insert(segment_to_patch_id[segment]).second) {
        return absl::InternalError(
            StrCat("Two different segments are mapped to the same patch: s",
                   segment, " -> p", segment_to_patch_id[segment]));
      }
    }
    bool is_fallback = (or_segments == fallback_group);
    segmentation.patches_.insert(std::pair(next_id, glyphs));
    segmentation.conditions_.insert(
        ActivationCondition::or_patches(or_patches, next_id, is_fallback));

    next_id++;
  }

  return absl::OkStatus();
}

StatusOr<uint32_t> PatchSizeBytes(hb_face_t* original_face,
                                  const absl::btree_set<glyph_id_t>& gids) {
  FontData font_data(original_face);
  CompatId id;
  GlyphKeyedDiff diff(font_data, id, {FontHelper::kGlyf, FontHelper::kGvar});
  auto patch_data = TRY(diff.CreatePatch(gids));
  return patch_data.size();
}

hb_set_unique_ptr ToSegmentIndices(const hb_set_t* patches,
                                   const std::vector<segment_index_t>& map) {
  hb_set_unique_ptr out = make_hb_set();
  patch_id_t next = HB_SET_VALUE_INVALID;
  while (hb_set_next(patches, &next)) {
    hb_set_add(out.get(), map[next]);
  }
  return out;
}

void MergeSegments(const SegmentationContext& context, const hb_set_t* segments,
                   hb_set_t* base) {
  segment_index_t next = HB_SET_VALUE_INVALID;
  while (hb_set_next(segments, &next)) {
    const hb_set_t* codepoints = context.segments[next].get();
    hb_set_union(base, codepoints);
  }
}

StatusOr<uint32_t> EstimatePatchSize(SegmentationContext& context,
                                     const hb_set_t* codepoints) {
  // TODO XXXXX keep a cache around for this
  hb_set_unique_ptr and_gids = make_hb_set();
  hb_set_unique_ptr or_gids = make_hb_set();
  hb_set_unique_ptr exclusive_gids = make_hb_set();
  TRYV(AnalyzeSegment(context, codepoints, and_gids.get(), or_gids.get(),
                      exclusive_gids.get()));

  auto btree_gids = to_btree_set(exclusive_gids.get());
  return PatchSizeBytes(context.original_face.get(), btree_gids);
}

StatusOr<std::optional<segment_index_t>> MergeNextBaseSegment(
    SegmentationContext& context,
    const GlyphSegmentation& candidate_segmentation,
    uint32_t patch_size_min_bytes, uint32_t patch_size_max_bytes) {
  printf("MergeNextBaseSegment():\n");
  // TODO XXXXX allow this function to start after a certain segment so we don't
  //            need to recheck what's already been processed.
  // TODO XXXX refactor and extract stuff into helpers to improve readability.

  hb_set_unique_ptr triggering_patches = make_hb_set();
  for (auto condition = candidate_segmentation.Conditions().begin();
       condition != candidate_segmentation.Conditions().end(); condition++) {
    if (!condition->IsExclusive()) {
      continue;
    }

    patch_id_t base_patch = condition->activated();
    segment_index_t base_segment_index =
        context.patch_id_to_segment_index[base_patch];
    printf("  checking patch size for %u (segment %u)\n", base_patch,
           base_segment_index);

    // step 1: measure the patch size for this segment.
    auto patch_glyphs = candidate_segmentation.GidSegments().find(base_patch);
    if (patch_glyphs == candidate_segmentation.GidSegments().end()) {
      return absl::InternalError(StrCat("patch ", base_patch, " not found."));
    }
    uint32_t patch_size_bytes =
        TRY(PatchSizeBytes(context.original_face.get(), patch_glyphs->second));
    if (patch_size_bytes >= patch_size_min_bytes) {
      continue;
    }

    // step 2: if below min locate candidate groups of segments to merge.
    printf("  patch %u (segment %u) is too small (%u < %u)\n", base_patch,
           base_segment_index, patch_size_bytes, patch_size_min_bytes);
    auto next_condition = condition;
    next_condition++;
    while (next_condition != candidate_segmentation.Conditions().end()) {
      if (next_condition->IsFallback()) {
        // Merging the fallback will cause all segments to be merged into one,
        // which is undesirable so don't consider the fallback.
        next_condition++;
        continue;
      }

      hb_set_clear(triggering_patches.get());
      next_condition->TriggeringPatches(triggering_patches.get());
      if (!hb_set_has(triggering_patches.get(), base_patch)) {
        next_condition++;
        continue;
      }

      printf("    try merging with: %s\n", next_condition->ToString().c_str());

      // Create a merged segment, and remove all of the others
      hb_set_unique_ptr to_merge_segments = ToSegmentIndices(
          triggering_patches.get(), context.patch_id_to_segment_index);
      hb_set_del(to_merge_segments.get(), base_segment_index);

      uint32_t size_before =
          hb_set_get_population(context.segments[base_segment_index].get());
      hb_set_unique_ptr merged_codepoints = make_hb_set();
      hb_set_union(merged_codepoints.get(),
                   context.segments[base_segment_index].get());
      MergeSegments(context, to_merge_segments.get(), merged_codepoints.get());

      uint32_t new_patch_size =
          TRY(EstimatePatchSize(context, merged_codepoints.get()));
      if (new_patch_size > patch_size_max_bytes) {
        printf("    skipping, patch would be too large (%u bytes)\n",
               new_patch_size);
        next_condition++;
        continue;
      }

      hb_set_union(context.segments[base_segment_index].get(),
                   merged_codepoints.get());
      uint32_t size_after =
          hb_set_get_population(context.segments[base_segment_index].get());
      printf(
          "    merged %u codepoints up to %u codepoints. New patch size %u "
          "bytes\n",
          size_before, size_after, new_patch_size);

      segment_index_t segment_index = HB_SET_VALUE_INVALID;
      while (hb_set_next(to_merge_segments.get(), &segment_index)) {
        // To avoid changing the indices of other segments set the ones we're
        // removing to empty sets. That effectively disables them.
        printf("    clearing segment %u\n", segment_index);
        hb_set_clear(context.segments[segment_index].get());
      }

      // Remove all segments we touched here from gid_conditions so they can be
      // recalculated.
      hb_set_add(to_merge_segments.get(), base_segment_index);
      for (auto& condition : context.gid_conditions) {
        condition.RemoveSegments(to_merge_segments.get());
      }

      // Return to the parent method so it can reanalyze and reform groups
      return base_segment_index;
    }

    // TODO XXXXXX if we didn't find any groupings that can be merged, then just
    // pick the next base segment to merge
    printf("    no composite to merge with.\n");
  }

  return std::nullopt;
}

StatusOr<GlyphSegmentation> GlyphSegmentation::CodepointToGlyphSegments(
    hb_face_t* face, flat_hash_set<hb_codepoint_t> initial_segment,
    std::vector<flat_hash_set<hb_codepoint_t>> codepoint_segments,
    uint32_t patch_size_min_bytes, uint32_t patch_size_max_bytes) {
  SegmentationContext context(face, initial_segment, codepoint_segments);

  segment_index_t segment_index = 0;
  for (const auto& segment : context.segments) {
    TRYV(AnalyzeSegment(context, segment_index, segment.get()));
    segment_index++;
  }

  while (true) {
    GlyphSegmentation segmentation;
    context.ResetGroupings();
    TRYV(GroupGlyphs(context));

    segmentation.unmapped_glyphs_ = context.unmapped_glyphs;
    segmentation.init_font_glyphs_ =
        to_btree_set(context.initial_closure.get());

    TRYV(GroupsToSegmentation(context.and_glyph_groups, context.or_glyph_groups,
                              context.fallback_segments,
                              context.patch_id_to_segment_index, segmentation));

    if (patch_size_min_bytes == 0) {
      return segmentation;
    }

    auto merged = TRY(MergeNextBaseSegment(
        context, segmentation, patch_size_min_bytes, patch_size_max_bytes));
    if (!merged.has_value()) {
      // Nothing was merged so we're done.
      return segmentation;
    }
    segment_index_t merged_segment_index = *merged;

    printf("Reanalyzing segment %u\n", merged_segment_index);
    TRYV(AnalyzeSegment(context, merged_segment_index,
                        context.segments[merged_segment_index].get()));
    printf("Done reanalyzing\n");
  }

  return absl::InternalError("unreachable");
}

GlyphSegmentation::ActivationCondition
GlyphSegmentation::ActivationCondition::and_patches(
    const absl::btree_set<patch_id_t>& ids, patch_id_t activated) {
  ActivationCondition conditions;
  conditions.activated_ = activated;

  for (auto id : ids) {
    conditions.conditions_.push_back({id});
  }

  return conditions;
}

GlyphSegmentation::ActivationCondition
GlyphSegmentation::ActivationCondition::or_patches(
    const absl::btree_set<patch_id_t>& ids, patch_id_t activated,
    bool is_fallback) {
  ActivationCondition conditions;
  conditions.activated_ = activated;
  conditions.conditions_.push_back(ids);
  conditions.is_fallback_ = is_fallback;

  return conditions;
}

template <typename It>
void output_set_inner(const char* prefix, const char* seperator, It begin,
                      It end, std::stringstream& out) {
  bool first = true;
  while (begin != end) {
    if (!first) {
      out << ", ";
    } else {
      first = false;
    }
    out << prefix << *(begin++);
  }
}

template <typename It>
void output_set(const char* prefix, It begin, It end, std::stringstream& out) {
  if (begin == end) {
    out << "{}";
    return;
  }

  out << "{ ";
  output_set_inner(prefix, ", ", begin, end, out);
  out << " }";
}

std::string GlyphSegmentation::ActivationCondition::ToString() const {
  std::stringstream out;
  out << "if (";
  bool first = true;
  for (const auto& set : conditions()) {
    if (!first) {
      out << " AND ";
    } else {
      first = false;
    }

    if (set.size() > 1) {
      out << "(";
    }
    bool first_inner = true;
    for (uint32_t id : set) {
      if (!first_inner) {
        out << " OR ";
      } else {
        first_inner = false;
      }
      out << "p" << id;
    }
    if (set.size() > 1) {
      out << ")";
    }
  }
  out << ") then p" << activated();
  return out.str();
}

std::string GlyphSegmentation::ToString() const {
  std::stringstream out;
  out << "initial font: ";
  output_set("gid", InitialFontGlyphs().begin(), InitialFontGlyphs().end(),
             out);
  out << std::endl;

  for (const auto& [segment_id, gids] : GidSegments()) {
    out << "p" << segment_id << ": ";
    output_set("gid", gids.begin(), gids.end(), out);
    out << std::endl;
  }

  for (const auto& condition : Conditions()) {
    out << condition.ToString() << std::endl;
  }

  return out.str();
}

bool GlyphSegmentation::ActivationCondition::operator<(
    const ActivationCondition& other) const {
  if (conditions_.size() != other.conditions_.size()) {
    return conditions_.size() < other.conditions_.size();
  }

  auto a = conditions_.begin();
  auto b = other.conditions_.begin();
  while (a != conditions_.end() && b != other.conditions_.end()) {
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

  if (activated_ != other.activated_) {
    return activated_ < other.activated_;
  }

  // These two are equal
  return false;
}

}  // namespace ift::encoder