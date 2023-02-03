#include "patch_subset/harfbuzz_subsetter.h"

#include <memory>

#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::StatusCode;

class HarfbuzzSubsetterTest : public ::testing::Test {
 protected:
  HarfbuzzSubsetterTest()
      : font_provider_(new FileFontProvider("patch_subset/testdata/")),
        subsetter_(std::make_unique<HarfbuzzSubsetter>()) {}

  std::unique_ptr<FontProvider> font_provider_;
  std::unique_ptr<Subsetter> subsetter_;
};

TEST_F(HarfbuzzSubsetterTest, Subset) {
  FontData font_data;
  EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.ttf", &font_data),
            StatusCode::kOk);

  hb_set_unique_ptr codepoints = make_hb_set_from_ranges(1, 0x61, 0x64);

  FontData subset_data;
  EXPECT_EQ(subsetter_->Subset(font_data, *codepoints, &subset_data),
            StatusCode::kOk);

  hb_blob_t* subset_blob =
      hb_blob_create(subset_data.data(), subset_data.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* subset_face = hb_face_create(subset_blob, 0);
  hb_blob_destroy(subset_blob);

  hb_set_unique_ptr subset_codepoints = make_hb_set();
  hb_face_collect_unicodes(subset_face, subset_codepoints.get());

  EXPECT_GT(hb_face_get_glyph_count(subset_face), 10);

  hb_face_destroy(subset_face);

  EXPECT_TRUE(hb_set_is_equal(codepoints.get(), subset_codepoints.get()));
}

TEST_F(HarfbuzzSubsetterTest, SubsetEmpty) {
  FontData font_data;
  EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.ttf", &font_data),
            StatusCode::kOk);

  hb_set_unique_ptr codepoints = make_hb_set(0);

  FontData subset_data;
  EXPECT_EQ(subsetter_->Subset(font_data, *codepoints, &subset_data),
            StatusCode::kOk);

  hb_blob_t* subset_blob =
      hb_blob_create(subset_data.data(), subset_data.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  EXPECT_EQ(hb_blob_get_length(subset_blob), 0);
  hb_blob_destroy(subset_blob);
}

TEST_F(HarfbuzzSubsetterTest, CodepointsInFont) {
  FontData font_data_1, font_data_2;
  EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Meows.ttf", &font_data_1),
            StatusCode::kOk);
  EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Awesome.ttf", &font_data_2),
            StatusCode::kOk);

  hb_set_unique_ptr expected = make_hb_set(5, 0x4D, 0x65, 0x6F, 0x77, 0x73);

  hb_set_unique_ptr result = make_hb_set();
  subsetter_->CodepointsInFont(font_data_1, result.get());
  EXPECT_TRUE(hb_set_is_equal(result.get(), expected.get()));

  expected = make_hb_set(6, 0x41, 0x65, 0x6D, 0x6F, 0x73, 0x77);
  result = make_hb_set();
  subsetter_->CodepointsInFont(font_data_2, result.get());
  EXPECT_TRUE(hb_set_is_equal(result.get(), expected.get()));
}

TEST_F(HarfbuzzSubsetterTest, CodepointsInFont_BadFont) {
  FontData font_data("not a font");

  hb_set_unique_ptr expected = make_hb_set();
  hb_set_unique_ptr result = make_hb_set();
  subsetter_->CodepointsInFont(font_data, result.get());

  EXPECT_TRUE(hb_set_is_equal(expected.get(), result.get()));
}

TEST_F(HarfbuzzSubsetterTest, SubsetNoRetainGids) {
  FontData font_data;
  EXPECT_EQ(font_provider_->GetFont("NotoSansJP-Regular.otf", &font_data),
            StatusCode::kOk);

  hb_set_unique_ptr codepoints = make_hb_set(1, 0xffed);

  FontData subset_data;
  EXPECT_EQ(subsetter_->Subset(font_data, *codepoints, &subset_data),
            StatusCode::kOk);

  hb_blob_t* subset_blob =
      hb_blob_create(subset_data.data(), subset_data.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* subset_face = hb_face_create(subset_blob, 0);
  hb_blob_destroy(subset_blob);

  hb_set_unique_ptr subset_codepoints = make_hb_set();
  hb_face_collect_unicodes(subset_face, subset_codepoints.get());

  EXPECT_EQ(hb_face_get_glyph_count(subset_face), 2);

  hb_face_destroy(subset_face);

  EXPECT_TRUE(hb_set_is_equal(codepoints.get(), subset_codepoints.get()));
}

}  // namespace patch_subset
