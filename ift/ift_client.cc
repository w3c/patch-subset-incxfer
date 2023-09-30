#include "ift/ift_client.h"

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/proto/ift_table.h"

using absl::btree_set;
using absl::StatusOr;
using patch_subset::FontData;
using patch_subset::proto::IFTTable;

namespace ift {

StatusOr<btree_set<std::string>> IFTClient::PatchUrlsFor(
    const FontData& font, const hb_set_t& additional_codepoints) const {
  hb_face_t* face = font.reference_face();
  auto ift = IFTTable::FromFont(face);
  hb_face_destroy(face);

  if (!ift.ok()) {
    return ift.status();
  }

  absl::btree_set<std::string> result;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(&additional_codepoints, &cp)) {
    auto v = ift->get_patch_map().find(cp);
    if (v == ift->get_patch_map().end()) {
      continue;
    }

    uint32_t patch_idx = v->second;
    result.insert(ift->chunk_to_url(patch_idx));
  }

  return result;
}

StatusOr<FontData> IFTClient::ApplyPatch(const FontData& font,
                                         const FontData& patch) const {
  // TODO
  return absl::InternalError("not implemented.");
}

}  // namespace ift