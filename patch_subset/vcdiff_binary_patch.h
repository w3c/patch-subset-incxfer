#ifndef PATCH_SUBSET_VCDIFF_BINARY_PATCH_H_
#define PATCH_SUBSET_VCDIFF_BINARY_PATCH_H_

#include "absl/status/status.h"
#include "common/binary_patch.h"
#include "common/font_data.h"

namespace patch_subset {

// Applies a patch that was created using vcdiff.
class VCDIFFBinaryPatch : public common::BinaryPatch {
 public:
  absl::Status Patch(const common::FontData& font_base,
                     const common::FontData& patch,
                     common::FontData* font_derived /* OUT */) const override;

  absl::Status Patch(const common::FontData& font_base,
                     const std::vector<common::FontData>& patch,
                     common::FontData* font_derived) const override;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_VCDIFF_BINARY_PATCH_H_
