#ifndef IFT_IFTB_BINARY_PATCH_H_
#define IFT_IFTB_BINARY_PATCH_H_

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/binary_patch.h"
#include "common/font_data.h"

namespace ift {

/* Applies one or more IFTB chunk file patches. */
class IftbBinaryPatch : public common::BinaryPatch {
 public:
  static absl::StatusOr<absl::flat_hash_set<uint32_t>> GidsInPatch(
      const common::FontData& patch);

  static absl::Status IdInPatch(const common::FontData& patch,
                                uint32_t id_out[4]);

  absl::Status Patch(const common::FontData& font_base,
                     const common::FontData& patch,
                     common::FontData* font_derived /* OUT */) const override;

  absl::Status Patch(const common::FontData& font_base,
                     const std::vector<common::FontData>& patch,
                     common::FontData* font_derived /* OUT */) const override;
};

}  // namespace ift

#endif  // IFT_IFTB_BINARY_PATCH_H_
