#ifndef PATCH_SUBSET_BINARY_PATCH_H_
#define PATCH_SUBSET_BINARY_PATCH_H_

#include "absl/status/status.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

// Interface to an object which applies a binary patch
// to a binary blob.
class BinaryPatch {
 public:
  virtual ~BinaryPatch() = default;

  // Apply a batch to font_base and write the results to font_derived.
  virtual absl::StatusCode Patch(const FontData& font_base, const FontData& patch,
                           FontData* font_derived) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BINARY_PATCH_H_
