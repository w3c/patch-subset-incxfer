#include "ift/ift_client.h"

#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/font_data.h"

using absl::StatusOr;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
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
    auto v = ift->get_patch_map().find(cp);
    if (v == ift->get_patch_map().end()) {
      continue;
    }

    uint32_t patch_idx = v->second.first;
    PatchEncoding encoding = v->second.second;
    result.insert(std::pair(ift->patch_to_url(patch_idx), encoding));
  }

  return result;
}

StatusOr<FontData> IFTClient::ApplyPatch(const FontData& font,
                                         const FontData& patch,
                                         PatchEncoding encoding) const {
  // TODO
  return absl::InternalError("not implemented.");
}

}  // namespace ift