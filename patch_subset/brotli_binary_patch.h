#ifndef PATCH_SUBSET_BROTLI_BINARY_PATCH_H_
#define PATCH_SUBSET_BROTLI_BINARY_PATCH_H_

#include "common/status.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

// Applies a patch that was created using brotli compression
// with a shared dictionary.
class BrotliBinaryPatch : public BinaryPatch {
 public:
  StatusCode Patch(const FontData& font_base, const FontData& patch,
                   FontData* font_derived /* OUT */) const override;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BROTLI_BINARY_PATCH_H_
