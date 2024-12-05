#include "ift/glyph_keyed_diff.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/compat_id.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb-subset.h"
#include "hb.h"
#include "ift/proto/ift_table.h"

using absl::flat_hash_set;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::CompatId;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::hb_font_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using common::make_hb_font;
using ift::proto::IFTTable;

namespace ift {

class IftbBinaryPatchTest : public ::testing::Test {
 protected:
  IftbBinaryPatchTest() {
    font = from_file("ift/testdata/NotoSansJP-Regular.ift.ttf");
    original = from_file("ift/testdata/NotoSansJP-Regular.subset.ttf");
    chunk1 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk1.br");
    chunk2 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk2.br");
    chunk3 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk3.br");
    chunk4 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk4.br");
  }

  FontData from_file(const char* filename) {
    return FontData(make_hb_blob(hb_blob_create_from_file(filename)));
  }

  FontData Subset(const FontData& font, flat_hash_set<uint32_t> gids) {
    hb_face_unique_ptr face = font.face();
    hb_subset_input_t* input = hb_subset_input_create_or_fail();
    for (uint32_t gid : gids) {
      hb_set_add(hb_subset_input_glyph_set(input), gid);
    }
    hb_subset_input_set_flags(
        input,
        HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED |
            HB_SUBSET_FLAGS_IFTB_REQUIREMENTS | HB_SUBSET_FLAGS_NOTDEF_OUTLINE);

    hb_subset_plan_t* plan = hb_subset_plan_create_or_fail(face.get(), input);
    hb_subset_input_destroy(input);

    hb_face_unique_ptr subset =
        make_hb_face(hb_subset_plan_execute_or_fail(plan));
    hb_subset_plan_destroy(plan);
    FontHelper::ApplyIftbTableOrdering(subset.get());

    hb_blob_unique_ptr subset_data =
        make_hb_blob(hb_face_reference_blob(subset.get()));

    FontData result(std::move(subset_data));
    return result;
  }

  FontData font;
  FontData original;
  FontData chunk1;
  FontData chunk2;
  FontData chunk3;
  FontData chunk4;
};

StatusOr<uint32_t> loca_value(string_view loca, hb_codepoint_t index) {
  string_view entry = loca.substr(index * 4, 4);
  return FontHelper::ReadUInt32(entry);
}

StatusOr<uint32_t> glyph_size(const FontData& font_data,
                              hb_codepoint_t codepoint) {
  hb_face_unique_ptr face = font_data.face();
  hb_font_unique_ptr font = make_hb_font(hb_font_create(face.get()));

  hb_codepoint_t gid;
  hb_font_get_nominal_glyph(font.get(), codepoint, &gid);
  if (gid == 0) {
    return absl::NotFoundError(StrCat("No cmap for ", codepoint));
  }

  auto loca = FontHelper::Loca(face.get());
  if (!loca.ok()) {
    return loca.status();
  }

  auto start = loca_value(*loca, gid);
  auto end = loca_value(*loca, gid + 1);

  if (!start.ok()) {
    return start.status();
  }
  if (!end.ok()) {
    return start.status();
  }

  return *end - *start;
}

TEST_F(IftbBinaryPatchTest, GidsInPatch) {
  auto gids = GlyphKeyedDiff::GidsInIftbPatch(chunk1);
  ASSERT_TRUE(gids.ok()) << gids.status();

  ASSERT_TRUE(gids->contains(313));
  ASSERT_TRUE(gids->contains(354));
  ASSERT_FALSE(gids->contains(71));
  ASSERT_FALSE(gids->contains(802));

  gids = GlyphKeyedDiff::GidsInIftbPatch(chunk4);
  ASSERT_TRUE(gids.ok()) << gids.status();

  ASSERT_TRUE(gids->contains(96));
  ASSERT_TRUE(gids->contains(765));
  ASSERT_TRUE(gids->contains(841));
  ASSERT_TRUE(gids->contains(1032));
  ASSERT_FALSE(gids->contains(313));
  ASSERT_FALSE(gids->contains(354));
}

TEST_F(IftbBinaryPatchTest, IdInPatch) {
  auto id = GlyphKeyedDiff::IdInIftbPatch(chunk1);
  ASSERT_TRUE(id.ok()) << id.status();
  ASSERT_EQ(*id, CompatId(0x3c2bfda0, 0x890625c9, 0x40c644de, 0xb1195627));
}

}  // namespace ift
