#ifndef COMMON_BROTLI_BINARY_PATCH_H_
#define COMMON_BROTLI_BINARY_PATCH_H_

#include "absl/status/status.h"
#include "common/binary_patch.h"
#include "common/font_data.h"

namespace common {

// Applies a patch that was created using brotli compression
// with a shared dictionary.
class BrotliBinaryPatch : public BinaryPatch {
 public:
  absl::Status Patch(const FontData& font_base, const FontData& patch,
                     FontData* font_derived /* OUT */) const override;

  absl::Status Patch(const FontData& font_base,
                     const std::vector<FontData>& patch,
                     FontData* font_derived) const override;
};

}  // namespace common

#endif  // COMMON_BROTLI_BINARY_PATCH_H_
