#include "ift/iftb_binary_patch.h"

#include <fstream>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb-subset.h"
#include "hb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/font_data.h"

using absl::flat_hash_set;
using absl::StatusOr;
using absl::StrCat;
using absl::string_view;
using common::FontHelper;
using ift::proto::IFTTable;
using patch_subset::FontData;

namespace ift {

class IftbBinaryPatchTest : public ::testing::Test {
 protected:
  IftbBinaryPatchTest() {
    font = from_file("ift/testdata/NotoSansJP-Regular.ift.ttf");
    chunk1 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk1.br");
    chunk2 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk2.br");
    chunk3 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk3.br");
    chunk4 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk4.br");
  }

  FontData from_file(const char* filename) {
    hb_blob_t* blob = hb_blob_create_from_file(filename);
    FontData result(blob);
    hb_blob_destroy(blob);
    return result;
  }

  FontData Subset(const FontData& font, flat_hash_set<uint32_t> gids) {
    hb_face_t* face = font.reference_face();
    hb_subset_input_t* input = hb_subset_input_create_or_fail();
    for (uint32_t gid : gids) {
      hb_set_add(hb_subset_input_glyph_set(input), gid);
    }
    hb_subset_input_set_flags(
        input,
        HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED |
            HB_SUBSET_FLAGS_IFTB_REQUIREMENTS | HB_SUBSET_FLAGS_NOTDEF_OUTLINE);

    hb_face_t* subset = hb_subset_or_fail(face, input);
    FontHelper::ApplyIftbTableOrdering(subset);

    FontData result(subset);

    hb_face_destroy(subset);
    hb_subset_input_destroy(input);
    hb_face_destroy(face);

    return result;
  }

  FontData font;
  FontData chunk1;
  FontData chunk2;
  FontData chunk3;
  FontData chunk4;
  IftbBinaryPatch patcher;
};

StatusOr<uint32_t> loca_value(string_view loca, hb_codepoint_t index) {
  string_view entry = loca.substr(index * 4, 4);
  return FontHelper::ReadUInt32(entry);
}

StatusOr<uint32_t> glyph_size(const FontData& font_data,
                              hb_codepoint_t codepoint) {
  hb_face_t* face = font_data.reference_face();
  hb_font_t* font = hb_font_create(face);

  hb_codepoint_t gid;
  hb_font_get_nominal_glyph(font, codepoint, &gid);
  if (gid == 0) {
    return absl::NotFoundError(StrCat("No cmap for ", codepoint));
  }

  auto loca = FontHelper::Loca(face);
  if (!loca.ok()) {
    hb_font_destroy(font);
    hb_face_destroy(face);
    return loca.status();
  }

  auto start = loca_value(*loca, gid);
  auto end = loca_value(*loca, gid + 1);

  hb_font_destroy(font);
  hb_face_destroy(face);

  if (!start.ok()) {
    return start.status();
  }
  if (!end.ok()) {
    return start.status();
  }

  return *end - *start;
}

TEST_F(IftbBinaryPatchTest, GidsInPatch) {
  auto gids = IftbBinaryPatch::GidsInPatch(chunk1);
  ASSERT_TRUE(gids.ok()) << gids.status();

  ASSERT_TRUE(gids->contains(313));
  ASSERT_TRUE(gids->contains(354));
  ASSERT_FALSE(gids->contains(71));
  ASSERT_FALSE(gids->contains(802));

  gids = IftbBinaryPatch::GidsInPatch(chunk4);
  ASSERT_TRUE(gids.ok()) << gids.status();

  ASSERT_TRUE(gids->contains(96));
  ASSERT_TRUE(gids->contains(765));
  ASSERT_TRUE(gids->contains(841));
  ASSERT_TRUE(gids->contains(1032));
  ASSERT_FALSE(gids->contains(313));
  ASSERT_FALSE(gids->contains(354));
}

TEST_F(IftbBinaryPatchTest, IdInPatch) {
  uint32_t id[4];
  auto sc = IftbBinaryPatch::IdInPatch(chunk1, id);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(id[0], 0x3c2bfda0);
  ASSERT_EQ(id[1], 0x890625c9);
  ASSERT_EQ(id[2], 0x40c644de);
  ASSERT_EQ(id[3], 0xb1195627);
}

TEST_F(IftbBinaryPatchTest, SinglePatch) {
  FontData result;
  auto s = patcher.Patch(font, chunk2, &result);
  ASSERT_TRUE(s.ok()) << s;
  ASSERT_GT(result.size(), 1000);

  auto ift_table = IFTTable::FromFont(result);
  ASSERT_TRUE(ift_table.ok()) << ift_table.status();

  for (const auto& e : ift_table->GetPatchMap().GetEntries()) {
    uint32_t patch_index = e.patch_index;
    for (uint32_t codepoint : e.coverage.codepoints) {
      ASSERT_NE(patch_index, 2);
      // spot check a couple of codepoints that should be removed.
      ASSERT_NE(codepoint, 0xa5);
      ASSERT_NE(codepoint, 0x30d4);
    }
  }

  ASSERT_EQ(*glyph_size(result, 0xab), 0);
  ASSERT_EQ(*glyph_size(result, 0x2e8d), 0);

  // TODO(garretrieger): check glyph is equal to corresponding glyph in the
  // original file.
  ASSERT_GT(*glyph_size(result, 0xa5), 1);
  ASSERT_LT(*glyph_size(result, 0xa5), 1000);
  ASSERT_GT(*glyph_size(result, 0x30d4), 1);
  ASSERT_LT(*glyph_size(result, 0x30d4), 1000);
}

TEST_F(IftbBinaryPatchTest, SinglePatchOnSubset) {
  auto gids = IftbBinaryPatch::GidsInPatch(chunk2);
  gids->insert(0);
  ASSERT_TRUE(gids.ok()) << gids.status();

  FontData subset = Subset(font, *gids);
  ASSERT_GT(subset.size(), 500);

  FontData result;
  auto s = patcher.Patch(subset, chunk2, &result);
  ASSERT_TRUE(s.ok()) << s;
  ASSERT_GT(result.size(), subset.size());
  auto sc = glyph_size(result, 0xa5);
  ASSERT_TRUE(sc.ok()) << sc.status();

  auto ift_table = IFTTable::FromFont(result);
  ASSERT_TRUE(ift_table.ok()) << ift_table.status();

  for (const auto& e : ift_table->GetPatchMap().GetEntries()) {
    uint32_t patch_index = e.patch_index;
    for (uint32_t codepoint : e.coverage.codepoints) {
      ASSERT_NE(patch_index, 2);
      // spot check a couple of codepoints that should be removed.
      ASSERT_NE(codepoint, 0xa5);
      ASSERT_NE(codepoint, 0x30d4);
    }
  }

  sc = glyph_size(result, 0xab);
  ASSERT_TRUE(absl::IsNotFound(sc.status())) << sc.status();
  sc = glyph_size(result, 0x2e8d);
  ASSERT_TRUE(absl::IsNotFound(sc.status())) << sc.status();

  // TODO(garretrieger): check glyph is equal to corresponding glyph in the
  // original file.
  ASSERT_GT(*glyph_size(result, 0xa5), 1);
  ASSERT_LT(*glyph_size(result, 0xa5), 1000);
  ASSERT_GT(*glyph_size(result, 0x30d4), 1);
  ASSERT_LT(*glyph_size(result, 0x30d4), 1000);
}

TEST_F(IftbBinaryPatchTest, MultiplePatches) {
  FontData result;
  std::vector<FontData> patches;
  patches.emplace_back().shallow_copy(chunk2);
  patches.emplace_back().shallow_copy(chunk3);
  auto s = patcher.Patch(font, patches, &result);
  ASSERT_TRUE(s.ok()) << s;
  ASSERT_GT(result.size(), 1000);

  auto ift_table = IFTTable::FromFont(result);
  ASSERT_TRUE(ift_table.ok()) << ift_table.status();

  for (const auto& e : ift_table->GetPatchMap().GetEntries()) {
    uint32_t patch_index = e.patch_index;
    for (uint32_t codepoint : e.coverage.codepoints) {
      ASSERT_NE(patch_index, 2);
      ASSERT_NE(patch_index, 3);
      // spot check a couple of codepoints that should be removed.
      ASSERT_NE(codepoint, 0xa5);
      ASSERT_NE(codepoint, 0xeb);
      ASSERT_NE(codepoint, 0x30d4);
    }
  }

  ASSERT_EQ(*glyph_size(result, 0xab), 0);
  ASSERT_EQ(*glyph_size(result, 0x2e8d), 0);

  // TODO(garretrieger): check glyph is equal to corresponding glyph in the
  // original file.
  ASSERT_GT(*glyph_size(result, 0xa5), 1);
  ASSERT_LT(*glyph_size(result, 0xa5), 1000);
  ASSERT_GT(*glyph_size(result, 0xeb), 1);
  ASSERT_LT(*glyph_size(result, 0xeb), 1000);
  ASSERT_GT(*glyph_size(result, 0x30d4), 1);
  ASSERT_LT(*glyph_size(result, 0x30d4), 1000);
}

TEST_F(IftbBinaryPatchTest, ConsecutivePatches) {
  FontData result1, result2, result_combined;
  auto s = patcher.Patch(font, chunk2, &result1);
  ASSERT_TRUE(s.ok()) << s;

  s = patcher.Patch(result1, chunk3, &result2);
  ASSERT_TRUE(s.ok()) << s;

  std::vector<FontData> patches;
  patches.emplace_back().shallow_copy(chunk2);
  patches.emplace_back().shallow_copy(chunk3);
  s = patcher.Patch(font, patches, &result_combined);
  ASSERT_TRUE(s.ok()) << s;

  ASSERT_EQ(result2.str(), result_combined.str());
}

TEST_F(IftbBinaryPatchTest, PatchesIdempotent) {
  FontData result1, result2;
  auto s = patcher.Patch(font, chunk2, &result1);
  ASSERT_TRUE(s.ok()) << s;

  s = patcher.Patch(result1, chunk2, &result2);
  ASSERT_TRUE(s.ok()) << s;

  ASSERT_EQ(result1.str(), result2.str());
}

}  // namespace ift
