#ifndef PATCH_SUBSET_BINARY_DIFF_H_
#define PATCH_SUBSET_BINARY_DIFF_H_

#include "absl/status/status.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

// Interface to an object which computes a binary diff between
// two binary blobs.
class BinaryDiff {
 public:
  virtual ~BinaryDiff() = default;

  // Compute a patch which can be applied to binary a to transform
  // it into binary b.
  virtual absl::StatusCode Diff(const FontData& font_base,
                                const FontData& font_derived,
                                FontData* patch /* OUT */) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BINARY_DIFF_H_
