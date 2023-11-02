#ifndef IFT_ENCODER_ENCODER_H_
#define IFT_ENCODER_ENCODER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
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
      : binary_diff_(11), per_table_binary_diff_({"IFT", "glyf", "loca"}) {}

  void SetUrlTemplate(const std::string& value) { url_template_ = value; }

  const std::string& UrlTemplate() const { return url_template_; }

  void AddExistingIftbPatch(uint32_t id,
                            const absl::flat_hash_set<uint32_t>& codepoints) {
    existing_iftb_patches_[id] = codepoints;
    if (id >= next_id_) {
      next_id_ = id + 1;
    }
  }

  absl::Span<const uint32_t> Id() const {
    // TODO(garretrieger): generate a new id on creation.
    constexpr static uint32_t id[4] = {1, 2, 3, 4};
    return id;
  }

  const absl::flat_hash_map<uint32_t, patch_subset::FontData>& Patches() const {
    return patches_;
  }

  /*
   * Create an IFT encoded version of 'font' that initially supports
   * 'base_subset' but can be extended via patches to support any combination of
   * 'subsets'.
   *
   * Returns: the IFT encoded initial font. Patches() will be populated with the
   * set of associated patch files.
   */
  absl::StatusOr<patch_subset::FontData> Encode(
      hb_face_t* font, const absl::flat_hash_set<hb_codepoint_t>& base_subset,
      std::vector<const absl::flat_hash_set<hb_codepoint_t>*> subsets,
      bool is_root = true);

  static absl::StatusOr<patch_subset::FontData> EncodeWoff2(
      absl::string_view font, bool glyf_transform = true);
  static absl::StatusOr<patch_subset::FontData> DecodeWoff2(
      absl::string_view font);
  static absl::StatusOr<patch_subset::FontData> RoundTripWoff2(
      absl::string_view font, bool glyf_transform = true);

 private:
  bool IsMixedMode() const { return !existing_iftb_patches_.empty(); }

  absl::StatusOr<patch_subset::FontData> CutSubset(
      hb_face_t* font, const absl::flat_hash_set<hb_codepoint_t>& codepoints);

  std::string url_template_ = "patch$5$4$3$2$1.br";
  absl::flat_hash_map<uint32_t, absl::flat_hash_set<uint32_t>>
      existing_iftb_patches_;

  uint32_t next_id_ = 0;
  patch_subset::BrotliBinaryDiff binary_diff_;
  ift::PerTableBrotliBinaryDiff per_table_binary_diff_;
  absl::flat_hash_map<absl::flat_hash_set<hb_codepoint_t>,
                      patch_subset::FontData>
      built_subsets_;
  absl::flat_hash_map<uint32_t, patch_subset::FontData> patches_;
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_ENCODER_H_