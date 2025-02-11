#ifndef IFT_ENCODER_ENCODER_H_
#define IFT_ENCODER_ENCODER_H_

#include <cstdint>
#include <initializer_list>
#include <random>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/axis_range.h"
#include "common/compat_id.h"
#include "common/font_data.h"
#include "hb-subset.h"
#include "ift/proto/patch_map.h"
#include "ift/table_keyed_diff.h"

namespace ift::encoder {

/*
 * Implementation of an encoder which can convert non-IFT fonts to an IFT
 * font and a set of patches.
 */
class Encoder {
 public:
  typedef absl::flat_hash_map<hb_tag_t, common::AxisRange> design_space_t;

  // TODO(garretrieger): add api to configure brotli quality level (for glyph
  // and table keyed).
  //                     Default to 11 but in tests run lower quality.

  // TODO XXXXXX be consistent with terminology used for patches/segments (ie.
  // standardize on one or the other throughout).

  /*
   * This conditions is satisfied if the input subset definition matches at
   * least one segment in each required group and every feature in
   * required_features.
   */
  struct Condition {
    std::vector<absl::btree_set<uint32_t>> required_groups;
    absl::btree_set<hb_tag_t> required_features;
    uint32_t activated_segment_id;

    Condition() : required_groups(), activated_segment_id(0) {}

    // Construct a condition that maps a segment to itself.
    Condition(uint32_t segment_id)
        : required_groups({{segment_id}}), activated_segment_id(segment_id) {}

    // Returns true if this condition is activated by exactly one segment and no
    // features.
    bool IsUnitary() const {
      return (required_groups.size() == 1) &&
             required_groups.at(0).size() == 1 && required_features.size() == 0;
    }

    bool operator<(const Condition& other) const;

    bool operator==(const Condition& other) const {
      return required_groups == other.required_groups &&
             required_features == other.required_features &&
             activated_segment_id == other.activated_segment_id;
    }

    friend void PrintTo(const Condition& point, std::ostream* os);
  };

  Encoder()
      : face_(common::make_hb_face(nullptr))

  {}

  Encoder(const Encoder&) = delete;
  Encoder(Encoder&& other) = delete;
  Encoder& operator=(const Encoder&) = delete;
  Encoder& operator=(Encoder&& other) = delete;

  /*
   * Configures how many graph levels can be reached from each node in the
   * encoded graph. Defaults to 1.
   */
  void SetJumpAhead(uint32_t count) { this->jump_ahead_ = count; }

  /*
   * Adds a segmentation of glyph data.
   *
   * In the generated encoding there will be one glyph keyed patch (containing
   * all data for all of the glyphs in the segment) per segment and unique
   * design space configuration.
   *
   * An id is provided which uniquely identifies this segment and can be used to
   * specify dependencies against this segment.
   */
  absl::Status AddGlyphDataSegment(uint32_t segment_id,
                                   const absl::flat_hash_set<uint32_t>& gids);

  // TODO(garretrieger): add a second type of activation condition which is
  // SubsetDefinition -> segment_id. That will be used to set up the base
  // activation conditions and then this method is only used for constructing
  // composite conditions. Stop inferering the subset definition from the gids
  // in a segment.
  absl::Status AddGlyphDataActivationCondition(Condition condition);

  /*
   * Marks that the segment identified by 'activated_id' will only be loaded if
   * 'feature_tag' is in the  target subset and the segment identified by
   * 'original_id' has been matched.
   *
   * The segments associated with 'original_id' and 'id' must have been
   * previously supplied via AddGlyphDataSegment().
   */
  absl::Status AddFeatureDependency(uint32_t original_id, uint32_t activated_id,
                                    hb_tag_t feature_tag);

  void SetFace(hb_face_t* face) { face_.reset(hb_face_reference(face)); }

  /*
   * Configure the base subset to cover the provided codepoints, and the set of
   * layout features retained by default in the harfbuzz subsetter.
   */
  absl::Status SetBaseSubset(
      const absl::flat_hash_set<hb_codepoint_t>& base_subset) {
    if (!base_subset_.empty()) {
      return absl::FailedPreconditionError("Base subset has already been set.");
    }
    base_subset_.codepoints = base_subset;
    return absl::OkStatus();
  }

  // Set up the base subset to cover all codepoints not listed in any glyph
  // segments plus codepoints and gids from any segments referenced in
  // 'included_segments'
  absl::Status SetBaseSubsetFromSegments(
      const absl::flat_hash_set<uint32_t>& included_segments);

  absl::Status SetBaseSubsetFromSegments(
      const absl::flat_hash_set<uint32_t>& included_segments,
      const design_space_t& design_space);

  void AddNonGlyphDataSegment(
      const absl::flat_hash_set<hb_codepoint_t>& subset) {
    SubsetDefinition def;
    def.codepoints = subset;
    extension_subsets_.push_back(def);
  }

  /*
   * Marks the provided group offeature tags as optional. In the dependent
   * patch graph it will be possible to add support for the features at any
   * node via a patch. Once enabled data for all codepoints and those features
   * will always be available.
   */
  void AddFeatureGroupSegment(const absl::btree_set<hb_tag_t>& feature_tag);

  void AddDesignSpaceSegment(const design_space_t& space);

  /*
   * Configure an extension subset for the non glyph dependent graph formed
   * from the glyphs available in one or more glyph data segments.
   */
  absl::Status AddNonGlyphSegmentFromGlyphSegments(
      const absl::flat_hash_set<uint32_t>& ids);

  struct Encoding {
    common::FontData init_font;
    absl::flat_hash_map<std::string, common::FontData> patches;
  };

  /*
   * Create an IFT encoded version of 'font' that initially supports
   * the configured base subset but can be extended via patches to support any
   * combination of of extension subsets.
   *
   * Returns: the IFT encoded initial font. Patches() will be populated with the
   * set of associated patch files.
   */
  absl::StatusOr<Encoding> Encode() const;

  // TODO(garretrieger): update handling of encoding for use in woff2,
  // see: https://w3c.github.io/IFT/Overview.html#ift-and-compression
  static absl::StatusOr<common::FontData> RoundTripWoff2(
      absl::string_view font, bool glyf_transform = true);

 public:
  struct SubsetDefinition {
    SubsetDefinition() {}
    SubsetDefinition(std::initializer_list<uint32_t> codepoints_in) {
      for (uint32_t cp : codepoints_in) {
        codepoints.insert(cp);
      }
    }

    friend void PrintTo(const SubsetDefinition& point, std::ostream* os);

    absl::flat_hash_set<uint32_t> codepoints;
    absl::flat_hash_set<uint32_t> gids;
    absl::btree_set<hb_tag_t> feature_tags;
    design_space_t design_space;

    bool IsVariable() const {
      for (const auto& [tag, range] : design_space) {
        if (range.IsRange()) {
          return true;
        }
      }
      return false;
    }

    bool empty() const {
      return codepoints.empty() && gids.empty() && feature_tags.empty() &&
             design_space.empty();
    }

    bool operator==(const SubsetDefinition& other) const {
      return codepoints == other.codepoints && gids == other.gids &&
             feature_tags == other.feature_tags &&
             design_space == other.design_space;
    }

    template <typename H>
    friend H AbslHashValue(H h, const SubsetDefinition& s) {
      return H::combine(std::move(h), s.codepoints, s.gids, s.feature_tags,
                        s.design_space);
    }

    void Union(const SubsetDefinition& other);

    void Subtract(const SubsetDefinition& other);

    void ConfigureInput(hb_subset_input_t* input, hb_face_t* face) const;

    ift::proto::PatchMap::Coverage ToCoverage() const;
  };

  absl::Status SetBaseSubsetFromDef(const SubsetDefinition& base_subset) {
    if (!base_subset_.empty()) {
      return absl::FailedPreconditionError("Base subset has already been set.");
    }
    base_subset_ = base_subset;
    return absl::OkStatus();
  }

  std::vector<SubsetDefinition> OutgoingEdges(const SubsetDefinition& base,
                                              uint32_t choose) const;

 private:
  struct ProcessingContext;

  // Returns the font subset which would be reach if all segments where added to
  // the font.
  absl::StatusOr<common::FontData> FullyExpandedSubset(
      const ProcessingContext& context) const;

  std::string UrlTemplate(uint32_t patch_set_id) const {
    if (patch_set_id == 0) {
      // patch_set_id 0 is always used for table keyed patches
      return "{id}.tk";
    }

    // All other ids are for glyph keyed.
    return absl::StrCat(patch_set_id, "_{id}.gk");
  }

  static void AddCombinations(const std::vector<const SubsetDefinition*>& in,
                              uint32_t number,
                              std::vector<SubsetDefinition>& out);

  SubsetDefinition Combine(const SubsetDefinition& s1,
                           const SubsetDefinition& s2) const;

  /*
   * Create an IFT encoded version of 'font' that initially supports
   * 'base_subset' but can be extended via patches to support any combination of
   * 'subsets'.
   *
   * Returns: the IFT encoded initial font. Patches() will be populated with the
   * set of associated patch files.
   */
  absl::StatusOr<common::FontData> Encode(ProcessingContext& context,
                                          const SubsetDefinition& base_subset,
                                          bool is_root = true) const;

  absl::StatusOr<SubsetDefinition> SubsetDefinitionForSegments(
      const absl::flat_hash_set<uint32_t>& ids) const;

  /*
   * Returns true if this encoding will contain both glyph keyed and table keyed
   * patches.
   */
  bool IsMixedMode() const { return !glyph_data_segments_.empty(); }

  absl::Status EnsureGlyphKeyedPatchesPopulated(
      ProcessingContext& context, const design_space_t& design_space,
      std::string& uri_template, common::CompatId& compat_id) const;

  absl::Status PopulateGlyphKeyedPatchMap(
      ift::proto::PatchMap& patch_map) const;

  absl::StatusOr<common::hb_face_unique_ptr> CutSubsetFaceBuilder(
      const ProcessingContext& context, hb_face_t* font,
      const SubsetDefinition& def) const;

  absl::StatusOr<common::FontData> GenerateBaseGvar(
      const ProcessingContext& context, hb_face_t* font,
      const design_space_t& design_space) const;

  void SetMixedModeSubsettingFlagsIfNeeded(const ProcessingContext& context,
                                           hb_subset_input_t* input) const;

  absl::StatusOr<common::FontData> CutSubset(const ProcessingContext& context,
                                             hb_face_t* font,
                                             const SubsetDefinition& def) const;

  absl::StatusOr<common::FontData> Instance(
      const ProcessingContext& context, hb_face_t* font,
      const design_space_t& design_space) const;

  absl::StatusOr<std::unique_ptr<const common::BinaryDiff>> GetDifferFor(
      const common::FontData& font_data, common::CompatId compat_id,
      bool replace_url_template) const;

  static ift::TableKeyedDiff* FullFontTableKeyedDiff(
      common::CompatId base_compat_id) {
    return new TableKeyedDiff(base_compat_id);
  }

  static ift::TableKeyedDiff* MixedModeTableKeyedDiff(
      common::CompatId base_compat_id) {
    return new TableKeyedDiff(base_compat_id, {"IFTX", "glyf", "loca", "gvar"});
  }

  static ift::TableKeyedDiff* ReplaceIftMapTableKeyedDiff(
      common::CompatId base_compat_id) {
    // the replacement differ is used during design space expansions, both
    // gvar and "IFT " are overwritten to be compatible with the new design
    // space. Glyph segment patches for all prev loaded glyphs will be
    // downloaded to repopulate variation data for existing glyphs.
    return new TableKeyedDiff(base_compat_id, {"glyf", "loca"},
                              {"IFTX", "gvar"});
  }

  bool AllocatePatchSet(ProcessingContext& context,
                        const design_space_t& design_space,
                        std::string& uri_template,
                        common::CompatId& compat_id) const;

  common::hb_face_unique_ptr face_;
  absl::btree_map<uint32_t, SubsetDefinition> glyph_data_segments_;

  absl::btree_set<Condition> activation_conditions_;

  SubsetDefinition base_subset_;
  std::vector<SubsetDefinition> extension_subsets_;
  uint32_t jump_ahead_ = 1;
  uint32_t next_id_ = 0;

  struct ProcessingContext {
    ProcessingContext(uint32_t next_id)
        : gen_(),
          random_values_(0, std::numeric_limits<uint32_t>::max()),
          next_id_(next_id) {}

    std::mt19937 gen_;
    std::uniform_int_distribution<uint32_t> random_values_;

    common::FontData fully_expanded_subset_;
    bool force_long_loca_and_gvar_ = false;

    uint32_t next_id_ = 0;
    uint32_t next_patch_set_id_ =
        1;  // id 0 is reserved for table keyed patches.
    absl::flat_hash_map<design_space_t, std::string> patch_set_uri_templates_;
    absl::flat_hash_map<design_space_t, common::CompatId>
        glyph_keyed_compat_ids_;

    absl::flat_hash_map<SubsetDefinition, common::FontData> built_subsets_;
    absl::flat_hash_map<std::string, common::FontData> patches_;

    common::CompatId GenerateCompatId();
  };
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_ENCODER_H_
