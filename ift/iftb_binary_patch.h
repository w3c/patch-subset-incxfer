#ifndef IFT_IFTB_BINARY_PATCH_H_
#define IFT_IFTB_BINARY_PATCH_H_

#include <vector>

#include "absl/status/status.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

namespace ift {

/* Applies one or more IFTB chunk file patches. */
class IftbBinaryPatch : public patch_subset::BinaryPatch {
 public:
  absl::Status Patch(
      const patch_subset::FontData& font_base,
      const patch_subset::FontData& patch,
      patch_subset::FontData* font_derived /* OUT */) const override;

  absl::Status Patch(
      const patch_subset::FontData& font_base,
      const std::vector<patch_subset::FontData>& patch,
      patch_subset::FontData* font_derived /* OUT */) const override;
};

}  // namespace ift

#endif  // IFT_IFTB_BINARY_PATCH_H_
