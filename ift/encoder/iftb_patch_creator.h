#ifndef IFT_ENCODER_IFTB_PATCH_CREATOR_H_
#define IFT_ENCODER_IFTB_PATCH_CREATOR_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/font_data.h"
#include "common/compat_id.h"

namespace ift::encoder {

/*
 * Implementation of an encoder which can convert non-IFT fonts to an IFT
 * font and a set of patches.
 *
 * Currently this only supports producing shared brotli IFT fonts. For IFTB
 * the util/iftb2ift.cc cli can be used to convert IFTB fonts into the IFT
 * format.
 */
class IftbPatchCreator {
 public:
  static absl::StatusOr<common::FontData> CreatePatch(
      const common::FontData& font, uint32_t chunk_idx,
      common::CompatId id, const absl::flat_hash_set<uint32_t>& gids);
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_IFTB_PATCH_CREATOR_H_
