#ifndef IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_
#define IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_

#include "absl/status/status.h"
#include "common/binary_patch.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"

namespace ift {

/* Creates a per table brotli binary diff of two fonts. */
class PerTableBrotliBinaryPatch : public common::BinaryPatch {
 public:
  PerTableBrotliBinaryPatch() {}

  absl::Status Patch(const common::FontData& font_base,
                     const common::FontData& patch,
                     common::FontData* font_derived) const override;

  // Apply a set of independent patches to font_base and write the results to
  // font_derived. will fail if the underlying patch is dependent.
  absl::Status Patch(const common::FontData& font_base,
                     const std::vector<common::FontData>& patch,
                     common::FontData* font_derived) const override;

 private:
  common::BrotliBinaryPatch binary_patch_;
};

}  // namespace ift

#endif  // IFT_PER_TABLE_BROTLI_BINARY_PATCH_H_
