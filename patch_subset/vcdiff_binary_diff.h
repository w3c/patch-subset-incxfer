#ifndef PATCH_SUBSET_VCDIFF_BINARY_DIFF_H_
#define PATCH_SUBSET_VCDIFF_BINARY_DIFF_H_

#include "absl/status/status.h"
#include "common/binary_diff.h"
#include "common/font_data.h"

namespace patch_subset {

// Computes a binary diff using VCDIFF
// (https://datatracker.ietf.org/doc/html/rfc3284)
class VCDIFFBinaryDiff : public common::BinaryDiff {
 public:
  absl::Status Diff(const common::FontData& font_base,
                    const common::FontData& font_derived,
                    common::FontData* patch /* OUT */) const override;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_VCDIFF_BINARY_DIFF_H_
