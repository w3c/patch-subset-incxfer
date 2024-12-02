#ifndef IFT_IFTB_BINARY_PATCH_H_
#define IFT_IFTB_BINARY_PATCH_H_

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/binary_diff.h"
#include "common/font_data.h"

namespace ift {

/* Applies one or more IFTB chunk file patches. */
class GlyphKeyedDiff : public common::BinaryDiff {
 public:
  static absl::StatusOr<absl::flat_hash_set<uint32_t>> GidsInIftbPatch(
      const common::FontData& patch);

  static absl::Status IdInIftbPatch(const common::FontData& patch,
                                    uint32_t id_out[4]);

  absl::Status Diff(const common::FontData& font_base,
                    const common::FontData& font_derived,
                    common::FontData* patch /* OUT */) const override;
};

}  // namespace ift

#endif  // IFT_IFTB_BINARY_PATCH_H_
