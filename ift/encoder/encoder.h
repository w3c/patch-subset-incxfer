#ifndef IFT_ENCODER_ENCODER_H_
#define IFT_ENCODER_ENCODER_H_

#include <initializer_list>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/axis_range.h"
#include "common/brotli_binary_diff.h"
#include "common/font_data.h"
#include "hb-subset.h"
#include "ift/per_table_brotli_binary_diff.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

namespace ift::encoder {

/*
 * Implementation of an encoder which can convert non-IFT fonts to an IFT
 * font and a set of patches.
 *
 * Currently this only supports producing shared brotli IFT fonts. For IFTB
 * the util/iftb2ift.cc cli can be used to convert IFTB fonts into the IFT
 * format.
 */
class Encoder {
 public:
  typedef absl::flat_hash_map<hb_tag_t, common::AxisRange> design_space_t;

  Encoder()
      : binary_diff_(11),
        per_table_binary_diff_({"IFT ", "glyf", "loca", "gvar"}),

        // the replacement differ is used during design space expansions, both
        // gvar and "IFT " are overwritten to be compatible with the new design
        // space. IFTB Patches for all prev loaded glyphs will be downloaded
        // to repopulate variation data for existing glyphs.
        replace_ift_map_binary_diff_({"glyf", "loca"}, {"IFT ", "gvar"}) {}

  ~Encoder() {
    if (face_) {
      hb_face_destroy(face_);
    }
  }

  Encoder(const Encoder&) = delete;
  Encoder(Encoder&& other) = delete;
  Encoder& operator=(const Encoder&) = delete;
  Encoder& operator=(Encoder&& other) = delete;

  void SetUrlTemplate(const std::string& value) { url_template_ = value; }

  const std::string& UrlTemplate() const { return url_template_; }

  /*
   * Configures how many graph levels can be reached from each node in the
   * encoded graph. Defaults to 1.
   */
  void SetJumpAhead(uint32_t count) { this->jump_ahead_ = count; }

  /*
   * Adds an IFTB patch to be included in the encoded font identified by 'id'
   * using the provided patch binary data.
   */
  absl::Status AddExistingIftbPatch(uint32_t id, const common::FontData& patch);

  /*
   * Adds an IFTB patch identified by 'id' that will only be loaded if
   * 'feature_tag' is in the  target subset and the IFTB patch 'original_id' has
   * been loaded.
   *
   * The patches associated with 'original_id' and 'id' must have been
   * previously supplied via AddExistingIftbPatch().
   */
  absl::Status AddIftbFeatureSpecificPatch(uint32_t original_id, uint32_t id,
                                           hb_tag_t feature_tag);

  /*
   * Overrides the patch url template used when the current subset matches
   * the specified design space. The url override only applies to the IFTB
   * mapping table.
   */
  void AddIftbUrlTemplateOverride(const design_space_t& design_space,
                                  absl::string_view url_template) {
    iftb_url_overrides_[design_space] = url_template;
  }

  void SetFace(hb_face_t* face) { face_ = hb_face_reference(face); }

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

  // Set up the base subset to cover all codepoints not listed in any IFTB
  // patches, plus codepoints from any patches referenced in 'included_patches'
  absl::Status SetBaseSubsetFromIftbPatches(
      const absl::flat_hash_set<uint32_t>& included_patches);

  absl::Status SetBaseSubsetFromIftbPatches(
      const absl::flat_hash_set<uint32_t>& included_patches,
      const design_space_t& design_space);

  void AddExtensionSubset(const absl::flat_hash_set<hb_codepoint_t>& subset) {
    SubsetDefinition def;
    def.codepoints = subset;
    extension_subsets_.push_back(def);
  }

  /*
   * Configure an extension subset for the dependent graph formed the glyphs
   * available in one or more IFTB patches (previously provided by calls too
   * AddExistingIftbPatch())
   */
  absl::Status AddExtensionSubsetOfIftbPatches(
      const absl::flat_hash_set<uint32_t>& ids);

  /*
   * Marks the provided group offeature tags as optional. In the dependent
   * patch graph it will be possible to add support for the features at any
   * node via a patch. Once enabled data for all codepoints and those features
   * will always be available.
   */
  void AddOptionalFeatureGroup(
      const absl::flat_hash_set<hb_tag_t>& feature_tag);

  void AddOptionalDesignSpace(const design_space_t& space);

  // TODO(garretrieger): add support for specifying IFTB patch + feature tag
  // mappings

  absl::Status SetId(absl::Span<const uint32_t> id) {
    if (id.size() != 4) {
      return absl::InvalidArgumentError("id must have size = 4.");
    }

    for (int i = 0; i < 4; i++) {
      id_[i] = id[i];
    }
    return absl::OkStatus();
  }

  absl::Span<const uint32_t> Id() const { return id_; }

  // TODO(garretrieger): just like with IFTClient transition to using urls as
  // the id.
  const absl::flat_hash_map<std::string, common::FontData>& Patches() const {
    return patches_;
  }

  /*
   * Create an IFT encoded version of 'font' that initially supports
   * the configured base subset but can be extended via patches to support any
   * combination of of extension subsets.
   *
   * Returns: the IFT encoded initial font. Patches() will be populated with the
   * set of associated patch files.
   */
  absl::StatusOr<common::FontData> Encode();

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
    absl::flat_hash_set<hb_tag_t> feature_tags;
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
  typedef absl::btree_map<uint32_t, SubsetDefinition> iftb_map;

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
  absl::StatusOr<common::FontData> Encode(const SubsetDefinition& base_subset,
                                          bool is_root = true);

  absl::StatusOr<SubsetDefinition> SubsetDefinitionForIftbPatches(
      const absl::flat_hash_set<uint32_t>& ids) const;

  bool IsMixedMode() const { return !existing_iftb_patches_.empty(); }

  absl::Status PopulateIftbPatchMap(ift::proto::PatchMap& patch_map,
                                    const design_space_t& design_space) const;

  absl::StatusOr<common::hb_face_unique_ptr> CutSubsetFaceBuilder(
      hb_face_t* font, const SubsetDefinition& def) const;

  absl::StatusOr<common::FontData> GenerateBaseGvar(
      hb_face_t* font, const design_space_t& design_space) const;

  absl::StatusOr<common::FontData> CutSubset(hb_face_t* font,
                                             const SubsetDefinition& def) const;

  template <typename T>
  void RemoveIftbPatches(T ids);

  absl::StatusOr<const common::BinaryDiff*> GetDifferFor(
      const ift::proto::IFTTable& base_table,
      const common::FontData& font_data) const;

  common::BrotliBinaryDiff binary_diff_;
  ift::PerTableBrotliBinaryDiff per_table_binary_diff_;
  ift::PerTableBrotliBinaryDiff replace_ift_map_binary_diff_;

  // == IN  ==
  std::string url_template_ = "patch$5$4$3$2$1.br";
  uint32_t id_[4] = {0, 0, 0, 0};
  hb_face_t* face_ = nullptr;
  absl::btree_map<uint32_t, SubsetDefinition> existing_iftb_patches_;

  // TODO(garretrieger): this likely needs to change once we are generating the
  // IFTB patches
  absl::flat_hash_map<uint32_t,
                      absl::flat_hash_map<hb_tag_t, absl::btree_set<uint32_t>>>
      iftb_feature_mappings_;

  SubsetDefinition base_subset_;
  std::vector<SubsetDefinition> extension_subsets_;
  absl::flat_hash_map<design_space_t, std::string> iftb_url_overrides_;
  uint32_t jump_ahead_ = 1;

  // == OUT ==

  // TODO(garretrieger): change to track next_id_ per template. Will be needed
  // once we are generating the IFTB patches.
  uint32_t next_id_ = 0;

  absl::flat_hash_map<SubsetDefinition, common::FontData> built_subsets_;
  absl::flat_hash_map<std::string, common::FontData> patches_;
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_ENCODER_H_
