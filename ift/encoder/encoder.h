#ifndef IFT_ENCODER_ENCODER_H_
#define IFT_ENCODER_ENCODER_H_

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "hb-subset.h"
#include "ift/per_table_brotli_binary_diff.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"

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
  Encoder()
      : binary_diff_(11), per_table_binary_diff_({"IFT ", "glyf", "loca"}) {}

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

  absl::Status AddExistingIftbPatch(uint32_t id,
                                    const patch_subset::FontData& patch);

  void SetFace(hb_face_t* face) { face_ = hb_face_reference(face); }

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

  void AddExtensionSubset(const absl::flat_hash_set<hb_codepoint_t>& subset) {
    SubsetDefinition def;
    def.codepoints = subset;
    extension_subsets_.push_back(def);
  }

  absl::Status AddExtensionSubsetOfIftbPatches(
      const absl::flat_hash_set<uint32_t>& ids);

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

  const absl::flat_hash_map<uint32_t, patch_subset::FontData>& Patches() const {
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
  absl::StatusOr<patch_subset::FontData> Encode();

  static absl::StatusOr<patch_subset::FontData> EncodeWoff2(
      absl::string_view font, bool glyf_transform = true);
  static absl::StatusOr<patch_subset::FontData> DecodeWoff2(
      absl::string_view font);
  static absl::StatusOr<patch_subset::FontData> RoundTripWoff2(
      absl::string_view font, bool glyf_transform = true);

 private:
  struct SubsetDefinition {
    absl::flat_hash_set<uint32_t> codepoints;
    absl::flat_hash_set<uint32_t> gids;

    bool empty() const { return codepoints.empty() && gids.empty(); }

    bool operator==(const SubsetDefinition& other) const {
      return codepoints == other.codepoints && gids == other.gids;
    }

    template <typename H>
    friend H AbslHashValue(H h, const SubsetDefinition& s) {
      return H::combine(std::move(h), s.codepoints, s.gids);
    }

    void Union(const SubsetDefinition& other);

    void ConfigureInput(hb_subset_input_t* input) const;
  };

  std::vector<const SubsetDefinition*> Remaining(
      const std::vector<const SubsetDefinition*>& subsets,
      const SubsetDefinition* subset) const;

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
  absl::StatusOr<patch_subset::FontData> Encode(
      const SubsetDefinition& base_subset,
      std::vector<const SubsetDefinition*> subsets, bool is_root = true);

  absl::StatusOr<SubsetDefinition> SubsetDefinitionForIftbPatches(
      const absl::flat_hash_set<uint32_t>& ids);

  bool IsMixedMode() const { return !existing_iftb_patches_.empty(); }

  absl::StatusOr<patch_subset::FontData> CutSubset(hb_face_t* font,
                                                   const SubsetDefinition& def);

  patch_subset::BrotliBinaryDiff binary_diff_;
  ift::PerTableBrotliBinaryDiff per_table_binary_diff_;

  // IN
  std::string url_template_ = "patch$5$4$3$2$1.br";
  uint32_t id_[4] = {0, 0, 0, 0};
  hb_face_t* face_ = nullptr;
  absl::btree_map<uint32_t, SubsetDefinition> existing_iftb_patches_;
  SubsetDefinition base_subset_;
  std::vector<SubsetDefinition> extension_subsets_;
  // TODO(garretrieger): also track additional gids that should be
  //  included in a subset (coming from the IFTB patches). implement
  //  by having a custom struct for subsets which as a gid and codepoint set.

  // OUT
  uint32_t next_id_ = 0;
  absl::flat_hash_map<SubsetDefinition, patch_subset::FontData> built_subsets_;
  absl::flat_hash_map<uint32_t, patch_subset::FontData> patches_;
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_ENCODER_H_