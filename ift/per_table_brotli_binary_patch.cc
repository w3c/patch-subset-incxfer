#include "ift/per_table_brotli_binary_patch.h"

#include "absl/container/flat_hash_set.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"

using absl::flat_hash_set;
using absl::Status;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::make_hb_face_builder;
using ift::proto::PerTablePatch;

namespace ift {

Status PerTableBrotliBinaryPatch::Patch(const FontData& font_base,
                                        const FontData& patch,
                                        FontData* font_derived) const {
  PerTablePatch proto;
  if (!proto.ParseFromArray(patch.data(), patch.size())) {
    return absl::InternalError("Failed to decode patch protobuf.");
  }

  flat_hash_set<hb_tag_t> replacements;
  for (const std::string& tag : proto.replaced_tables()) {
    replacements.insert(FontHelper::ToTag(tag));
  }

  hb_face_unique_ptr base = font_base.face();
  auto tags = FontHelper::GetTags(base.get());

  // Some tags might be new so add all tags in the patch's table list.
  for (const auto& e : proto.table_patches()) {
    const std::string& tag = e.first;
    tags.insert(HB_TAG(tag[0], tag[1], tag[2], tag[3]));
  }

  // Remove any tags that are marked for removal.
  for (std::string tag : proto.removed_tables()) {
    tags.erase(HB_TAG(tag[0], tag[1], tag[2], tag[3]));
  }

  hb_face_unique_ptr new_face = make_hb_face_builder();
  for (hb_tag_t t : tags) {
    FontData data;
    if (!replacements.contains(t)) {
      data = FontHelper::TableData(base.get(), t);
    }
    FontData patch;

    FontData derived;
    std::string tag = FontHelper::ToString(t);
    auto it = proto.table_patches().find(tag);
    if (it != proto.table_patches().end()) {
      const std::string& patch_data = it->second;
      patch.copy(patch_data.data(), patch_data.size());

      auto sc = binary_patch_.Patch(data, patch, &derived);
      if (!sc.ok()) {
        return sc;
      }
    } else {
      // No patch for this table, just pass it through.
      derived.shallow_copy(data);
    }

    hb_blob_unique_ptr blob = derived.blob();
    hb_face_builder_add_table(new_face.get(), t, blob.get());
  }

  FontHelper::ApplyIftbTableOrdering(new_face.get());

  hb_blob_unique_ptr new_face_blob =
      common::make_hb_blob(hb_face_reference_blob(new_face.get()));
  font_derived->set(new_face_blob.get());

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
