#ifndef PATCH_SUBSET_BROTLI_BINARY_DIFF_H_
#define PATCH_SUBSET_BROTLI_BINARY_DIFF_H_

#include <vector>

#include "common/status.h"
#include "patch_subset/binary_diff.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

// Computes a binary diff using brotli compression
// with a shared dictionary.
class BrotliBinaryDiff : public BinaryDiff {
 public:
  StatusCode Diff(const FontData& font_base, const FontData& font_derived,
                  FontData* patch /* OUT */) const override;

  // For use in stitching together a brotli patch.
  StatusCode Diff(const FontData& font_base,
                  ::absl::string_view data,
                  unsigned stream_offset,
                  bool is_last,
                  std::vector<uint8_t>& sink) const;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BROTLI_BINARY_DIFF_H_
