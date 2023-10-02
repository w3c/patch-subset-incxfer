#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/proto/IFT.pb.h"

namespace ift {

typedef absl::btree_set<
    std::pair<std::string, patch_subset::proto::PatchEncoding>>
    patch_set;

class IFTClient {
 public:
  absl::StatusOr<patch_set> PatchUrlsFor(
      const patch_subset::FontData& font,
      const hb_set_t& additional_codepoints) const;

  absl::StatusOr<patch_subset::FontData> ApplyPatch(
      const patch_subset::FontData& font, const patch_subset::FontData& patch,
      patch_subset::proto::PatchEncoding encoding) const;
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_