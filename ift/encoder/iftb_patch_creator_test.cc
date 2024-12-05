#include "ift/encoder/iftb_patch_creator.h"

#include "absl/strings/str_cat.h"
#include "common/font_data.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/glyph_keyed_diff.h"
#include "merger.h"

using absl::StrCat;
using common::FontData;
using common::make_hb_blob;
using ift::GlyphKeyedDiff;

namespace ift::encoder {

class IftbPatchCreatorTest : public ::testing::Test {
 protected:
  IftbPatchCreatorTest() {
    auto blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP-Regular.subset.ttf"));
    base_font_.set(blob.get());

    blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP[wght].subset.ttf"));
    vf_font_.set(blob.get());

    for (int i = 1; i <= 4; i++) {
      std::string name =
          StrCat("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk", i, ".br");
      auto blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      iftb_patches_[i].set(blob.get());

      name =
          StrCat("ift/testdata/NotoSansJP[wght].subset_iftb/chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      vf_iftb_patches_[i].set(blob.get());
    }
  }

  FontData base_font_;
  FontData vf_font_;
  FontData iftb_patches_[5];
  FontData vf_iftb_patches_[5];
};

TEST_F(IftbPatchCreatorTest, GlyfOnly) {
  auto gids = GlyphKeyedDiff::GidsInIftbPatch(iftb_patches_[2]);

  auto patch = IftbPatchCreator::CreatePatch(
      base_font_, 2, {0x3c2bfda0u, 0x890625c9u, 0x40c644deu, 0xb1195627u},
      *gids);
  ASSERT_TRUE(patch.ok());

  std::string generated =
      iftb::decodeChunk(iftb_patches_[2].data(), iftb_patches_[2].size());
  std::string existing = iftb::decodeChunk(patch->data(), patch->size());

  ASSERT_GT(generated.size(), 1000);
  ASSERT_EQ(generated, existing);
}

TEST_F(IftbPatchCreatorTest, GlyfAndGvar) {
  auto gids = GlyphKeyedDiff::GidsInIftbPatch(vf_iftb_patches_[2]);

  auto patch = IftbPatchCreator::CreatePatch(
      vf_font_, 2, {0x479bb4b0u, 0x20226239u, 0xa7799c0fu, 0x24275be0u}, *gids);
  ASSERT_TRUE(patch.ok()) << patch.status();

  std::string generated =
      iftb::decodeChunk(vf_iftb_patches_[2].data(), vf_iftb_patches_[2].size());
  std::string existing = iftb::decodeChunk(patch->data(), patch->size());

  ASSERT_GT(generated.size(), 1000);
  ASSERT_EQ(generated, existing);
}

}  // namespace ift::encoder