#include "ift/ift_client.h"

#include <sstream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

using absl::Status;
using absl::StatusOr;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::IFTB_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BinaryPatch;
using patch_subset::FontData;

namespace ift {

StatusOr<patch_set> IFTClient::PatchUrlsFor(
    const FontData& font, const hb_set_t& additional_codepoints) const {
  hb_face_t* face = font.reference_face();
  auto ift = IFTTable::FromFont(face);
  hb_face_destroy(face);

  if (!ift.ok()) {
    return ift.status();
  }

  patch_set result;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(&additional_codepoints, &cp)) {
    auto v = ift->GetPatchMap().find(cp);
    if (v == ift->GetPatchMap().end()) {
      continue;
    }

    uint32_t patch_idx = v->second.first;
    PatchEncoding encoding = v->second.second;
    result.insert(std::pair(ift->PatchToUrl(patch_idx), encoding));
  }

  return result;
}

StatusOr<FontData> IFTClient::ApplyPatches(const FontData& font,
                                           const std::vector<FontData>& patches,
                                           PatchEncoding encoding) const {
  auto patcher = PatcherFor(encoding);
  if (!patcher.ok()) {
    return patcher.status();
  }

  FontData result;
  Status s = (*patcher)->Patch(font, patches, &result);
  if (!s.ok()) {
    return s;
  }

  return result;
}

StatusOr<const BinaryPatch*> IFTClient::PatcherFor(
    ift::proto::PatchEncoding encoding) const {
  switch (encoding) {
    case SHARED_BROTLI_ENCODING:
      return brotli_binary_patch_.get();
    case IFTB_ENCODING:
      return iftb_binary_patch_.get();
    default:
      std::stringstream message;
      message << "Patch encoding " << encoding << " is not implemented.";
      return absl::UnimplementedError(message.str());
  }
}

}  // namespace ift