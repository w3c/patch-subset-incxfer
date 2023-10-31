#ifndef IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_
#define IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_

#include "absl/status/status.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"

namespace ift {

/* Creates a per table brotli binary diff of two fonts. */
class PerTableBrotliBinaryPatch : public patch_subset::BinaryPatch {
 public:
  PerTableBrotliBinaryPatch() {}

  absl::Status Patch(const patch_subset::FontData& font_base,
                     const patch_subset::FontData& patch,
                     patch_subset::FontData* font_derived) const override;

  // Apply a set of independent patches to font_base and write the results to
  // font_derived. will fail if the underlying patch is dependent.
  absl::Status Patch(const patch_subset::FontData& font_base,
                     const std::vector<patch_subset::FontData>& patch,
                     patch_subset::FontData* font_derived) const override;

 private:
  patch_subset::BrotliBinaryPatch binary_patch_;
};

}  // namespace ift

#endif  // IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_
