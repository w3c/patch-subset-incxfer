#ifndef IFT_ENCODER_ENCODER_H_
#define IFT_ENCODER_ENCODER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"

namespace ift::encoder {

class Encoder {
 public:
  void SetUrlTemplate(const std::string& value) { url_template_ = value; }

  const std::string& UrlTemplate() const { return url_template_; }

  absl::Span<const uint32_t> Id() const {
    // TODO(garretrieger): generate a new id on creation.
    const uint32_t id[4] = {1, 2, 3, 4};
    return id;
  }

  const absl::flat_hash_map<uint32_t, patch_subset::FontData>& Patches() const {
    return patches_;
  }

  absl::StatusOr<patch_subset::FontData> Encode(
      hb_face_t* font, const absl::flat_hash_set<hb_codepoint_t>& base_subset,
      // TODO(garretrieger): do we need a pointer here for the subset members?
      std::vector<const absl::flat_hash_set<hb_codepoint_t>*> subsets);

 private:
  absl::StatusOr<patch_subset::FontData> CutSubset(
      hb_face_t* font, const absl::flat_hash_set<hb_codepoint_t>& codepoints);

  std::string url_template_ = "patch$5$4$3$2$1.br";
  uint32_t next_id_ = 0;
  patch_subset::BrotliBinaryDiff binary_diff_;
  absl::flat_hash_map<absl::flat_hash_set<hb_codepoint_t>,
                      patch_subset::FontData>
      built_subsets_;
  absl::flat_hash_map<uint32_t, patch_subset::FontData> patches_;
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_ENCODER_H_