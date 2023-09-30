#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/font_data.h"

namespace ift {

class IFTClient {
  absl::StatusOr<absl::btree_set<std::string>> PatchUrlsFor(
      const patch_subset::FontData& font,
      const hb_set_t& additional_codepoints) const;

  absl::StatusOr<patch_subset::FontData> ApplyPatch(
      const patch_subset::FontData& font,
      const patch_subset::FontData& patch) const;
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_