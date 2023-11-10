#include "ift/per_table_brotli_binary_patch.h"

#include "common/font_helper.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"

using absl::Status;
using common::FontHelper;
using ift::proto::PerTablePatch;
using patch_subset::FontData;

namespace ift {

Status PerTableBrotliBinaryPatch::Patch(const FontData& font_base,
                                        const FontData& patch,
                                        FontData* font_derived) const {
  PerTablePatch proto;
  if (!proto.ParseFromArray(patch.data(), patch.size())) {
    return absl::InternalError("Failed to decode patch protobuf.");
  }

  hb_face_t* base = font_base.reference_face();
  auto tags = FontHelper::GetTags(base);

  // Some tags might be new so add all tags in the patch's table list.
  for (const auto& e : proto.table_patches()) {
    const std::string& tag = e.first;
    tags.insert(HB_TAG(tag[0], tag[1], tag[2], tag[3]));
  }

  // Remove any tags that are marked for removal.
  for (std::string tag : proto.removed_tables()) {
    tags.erase(HB_TAG(tag[0], tag[1], tag[2], tag[3]));
  }

  hb_face_t* new_face = hb_face_builder_create();
  for (hb_tag_t t : tags) {
    FontData data = FontHelper::TableData(base, t);
    FontData patch;

    FontData derived;
    std::string tag = FontHelper::ToString(t);
    auto it = proto.table_patches().find(tag);
    if (it != proto.table_patches().end()) {
      const std::string& patch_data = it->second;
      patch.copy(patch_data.data(), patch_data.size());

      auto sc = binary_patch_.Patch(data, patch, &derived);
      if (!sc.ok()) {
        hb_face_destroy(base);
        hb_face_destroy(new_face);
        return sc;
      }
    } else {
      // No patch for this table, just pass it through.
      derived.shallow_copy(data);
    }

    hb_blob_t* blob = derived.reference_blob();
    hb_face_builder_add_table(new_face, t, blob);
    hb_blob_destroy(blob);
  }

  FontHelper::ApplyIftbTableOrdering(new_face);

  hb_blob_t* new_face_blob = hb_face_reference_blob(new_face);
  font_derived->set(new_face_blob);
  hb_blob_destroy(new_face_blob);
  hb_face_destroy(base);

  return absl::OkStatus();
}

Status PerTableBrotliBinaryPatch::Patch(const FontData& font_base,
                                        const std::vector<FontData>& patch,
                                        FontData* font_derived) const {
  if (patch.size() == 1) {
    return Patch(font_base, patch[0], font_derived);
  }

  if (patch.size() == 0) {
    return absl::InvalidArgumentError("Must provide at least one patch.");
  }

  return absl::InvalidArgumentError(
      "Per table brotli binary patches cannot be applied independently.");
}

}  // namespace ift