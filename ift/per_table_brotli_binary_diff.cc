#include "ift/per_table_brotli_binary_diff.h"

#include "absl/container/flat_hash_set.h"
#include "common/font_helper.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::Status;
using common::FontHelper;
using ift::proto::PerTablePatch;
using patch_subset::FontData;

namespace ift {

Status PerTableBrotliBinaryDiff::Diff(const FontData& font_base,
                                      const FontData& font_derived,
                                      FontData* patch /* OUT */) const {
  hb_face_t* face_base = font_base.reference_face();
  hb_face_t* face_derived = font_derived.reference_face();

  auto base_tags = FontHelper::GetTags(face_base);
  auto derived_tags = FontHelper::GetTags(face_derived);
  auto diff_tags = TagsToDiff(base_tags, derived_tags);

  PerTablePatch patch_proto;

  for (std::string tag : diff_tags) {
    hb_tag_t t = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
    bool in_base = base_tags.contains(t);
    bool in_derived = derived_tags.contains(t);

    if (in_base && !in_derived) {
      patch_proto.add_removed_tables(tag);
      continue;
    }

    FontData base_table = FontHelper::TableData(face_base, t);
    FontData derived_table = FontHelper::TableData(face_derived, t);
    FontData table_patch;
    auto sc = binary_diff_.Diff(base_table, derived_table, &table_patch);
    if (!sc.ok()) {
      hb_face_destroy(face_base);
      hb_face_destroy(face_derived);
      return sc;
    }

    (*patch_proto.mutable_table_patches())[tag] = table_patch.str();
  }

  hb_face_destroy(face_base);
  hb_face_destroy(face_derived);

  std::string patch_data = patch_proto.SerializeAsString();
  patch->copy(patch_data);

  return absl::OkStatus();
}

void PerTableBrotliBinaryDiff::AddAllMatching(
    const flat_hash_set<uint32_t>& tags, btree_set<std::string>& result) const {
  for (const uint32_t& t : tags) {
    std::string tag = FontHelper::ToString(t);
    if (target_tags_.empty() || target_tags_.contains(tag)) {
      result.insert(tag);
    }
  }
}

btree_set<std::string> PerTableBrotliBinaryDiff::TagsToDiff(
    const absl::flat_hash_set<uint32_t>& before,
    const absl::flat_hash_set<uint32_t>& after) const {
  btree_set<std::string> result;
  AddAllMatching(before, result);
  AddAllMatching(after, result);
  return result;
}

}  // namespace ift