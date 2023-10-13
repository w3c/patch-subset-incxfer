#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include "absl/container/btree_map.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/iftb_binary_patch.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"

namespace ift {

typedef absl::btree_map<std::string, ift::proto::PatchEncoding> patch_set;

/*
 * Client library for IFT fonts. Provides common operations needed by a client
 * trying to use an IFT font.
 */
class IFTClient {
 public:
  IFTClient()
      : brotli_binary_patch_(new patch_subset::BrotliBinaryPatch()),
        iftb_binary_patch_(new ift::IftbBinaryPatch()) {}

  static std::string PatchToUrl(const std::string& url_template,
                                uint32_t patch_idx);

  /*
   * Returns the set of patches needed to add support for all codepoints
   * in 'additional_codepoints'.
   */
  absl::StatusOr<patch_set> PatchUrlsFor(
      const patch_subset::FontData& font,
      const hb_set_t& additional_codepoints) const;

  /*
   * Applies one or more 'patches' to 'font'. The patches are all encoded
   * in the 'encoding' format.
   *
   * Returns the extended font.
   */
  absl::StatusOr<patch_subset::FontData> ApplyPatches(
      const patch_subset::FontData& font,
      const std::vector<patch_subset::FontData>& patches,
      ift::proto::PatchEncoding encoding) const;

 private:
  absl::StatusOr<const patch_subset::BinaryPatch*> PatcherFor(
      ift::proto::PatchEncoding encoding) const;

  std::unique_ptr<patch_subset::BinaryPatch> brotli_binary_patch_;
  std::unique_ptr<ift::IftbBinaryPatch> iftb_binary_patch_;
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_