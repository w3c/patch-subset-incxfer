#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/iftb_binary_patch.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/font_data.h"

using absl::StatusOr;
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

TEST_F(IftbBinaryPatchTest, SinglePatch) {
  FontData result;
  auto s = patcher.Patch(font, chunk2, &result);
  ASSERT_TRUE(s.ok()) << s;
  ASSERT_GT(result.size(), 1000);

  auto ift_table = IFTTable::FromFont(result);
  ASSERT_TRUE(ift_table.ok()) << ift_table.status();

  for (auto e : ift_table->GetPatchMap()) {
    uint32_t codepoint = e.first;
    uint32_t patch_index = e.second.first;
    ASSERT_NE(patch_index, 2);
    // spot check a couple of codepoints that should be removed.
    ASSERT_NE(codepoint, 0xa5);
    ASSERT_NE(codepoint, 0x30d4);
  }

  ASSERT_EQ(*glyph_size(result, 0xab), 0);
  ASSERT_EQ(*glyph_size(result, 0x2e8d), 0);

  // TODO(garretrieger): check glyph is equal to corresponding glyph in the
  // original file.
  ASSERT_GT(*glyph_size(result, 0xa5), 1);
  ASSERT_GT(*glyph_size(result, 0x30d4), 1);
}

}  // namespace ift
