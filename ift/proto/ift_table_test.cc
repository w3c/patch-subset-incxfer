#include "ift/proto/ift_table.h"

#include <cstdio>
#include <cstring>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "gtest/gtest.h"
#include "ift/proto/IFT.pb.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::hb_set_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using common::make_hb_set;
using common::SparseBitSet;

namespace ift::proto {

class IFTTableTest : public ::testing::Test {
 protected:
  IFTTableTest()
      : roboto_ab(make_hb_face(nullptr)), iftb(make_hb_face(nullptr)) {
    sample.SetUrlTemplate("fonts/go/here");
    auto ignore = sample.SetId({1, 2, 3, 4});
    sample.GetPatchMap().AddEntry({30, 32}, 1, SHARED_BROTLI_ENCODING);
    sample.GetPatchMap().AddEntry({55, 56, 57}, 2, IFTB_ENCODING);

    sample_with_extensions = sample;
    sample_with_extensions.GetPatchMap().AddEntry({77, 78}, 3,
                                                  SHARED_BROTLI_ENCODING, true);

    overlap_sample = sample;
    overlap_sample.GetPatchMap().AddEntry({55}, 3, SHARED_BROTLI_ENCODING);

    complex_ids.SetUrlTemplate("fonts/go/here");
    complex_ids.GetPatchMap().AddEntry({0}, 0, SHARED_BROTLI_ENCODING);
    complex_ids.GetPatchMap().AddEntry({5}, 5, SHARED_BROTLI_ENCODING);
    complex_ids.GetPatchMap().AddEntry({2}, 2, SHARED_BROTLI_ENCODING);
    complex_ids.GetPatchMap().AddEntry({4}, 4, SHARED_BROTLI_ENCODING);

    hb_blob_unique_ptr blob = make_hb_blob(hb_blob_create_from_file(
        "patch_subset/testdata/Roboto-Regular.ab.ttf"));
    roboto_ab = make_hb_face(hb_face_create(blob.get(), 0));

    blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP-Regular.iftb.ttf"));
    FontData font_data(std::move(blob));

    std::string copy = font_data.string();
    copy[0] = 'O';
    copy[1] = 'T';
    copy[2] = 'T';
    copy[3] = 'O';

    font_data.copy(copy);
    iftb = font_data.face();
  }

  hb_face_unique_ptr roboto_ab;
  hb_face_unique_ptr iftb;
  IFTTable empty;
  IFTTable sample;
  IFTTable sample_with_extensions;
  IFTTable overlap_sample;
  IFTTable complex_ids;
};

TEST_F(IFTTableTest, AddToFont) {
  auto font = sample.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  hb_blob_get_data(blob.get(), &length);
  EXPECT_GT(length, 0);

  auto original_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(roboto_ab.get()));
  auto new_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(face.get()));

  new_tag_order.erase(
      std::find(new_tag_order.begin(), new_tag_order.end(), "IFT "));

  EXPECT_EQ(original_tag_order, new_tag_order);
}

TEST_F(IFTTableTest, AddToFont_WithExtension) {
  auto font = sample_with_extensions.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();
  hb_face_unique_ptr face = font->face();

  FontData ift_table =
      FontHelper::TableData(face.get(), HB_TAG('I', 'F', 'T', ' '));
  FontData iftx_table =
      FontHelper::TableData(face.get(), HB_TAG('I', 'F', 'T', 'X'));

  ASSERT_GT(ift_table.size(), 1);
  ASSERT_GT(iftx_table.size(), 1);

  auto original_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(roboto_ab.get()));
  auto new_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(face.get()));

  new_tag_order.erase(
      std::find(new_tag_order.begin(), new_tag_order.end(), "IFT "));
  new_tag_order.erase(
      std::find(new_tag_order.begin(), new_tag_order.end(), "IFTX"));

  EXPECT_EQ(original_tag_order, new_tag_order);
}

TEST_F(IFTTableTest, RoundTrip_Sample) {
  auto font = sample.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  auto table = IFTTable::FromFont(*font);
  ASSERT_TRUE(table.ok()) << table.status();

  ASSERT_EQ(*table, sample);
}

TEST_F(IFTTableTest, RoundTrip_ComplexIds) {
  auto font = complex_ids.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  auto table = IFTTable::FromFont(*font);
  ASSERT_TRUE(table.ok()) << table.status();

  ASSERT_EQ(*table, complex_ids);
}

TEST_F(IFTTableTest, RoundTrip_Overlaps) {
  auto font = overlap_sample.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  auto table = IFTTable::FromFont(*font);
  ASSERT_TRUE(table.ok()) << table.status();

  ASSERT_EQ(*table, overlap_sample);
}

TEST_F(IFTTableTest, RoundTrip_WithExtension) {
  IFTTable table;
  table.SetUrlTemplate("files/go/here/$1.br");
  table.GetPatchMap().AddEntry({10}, 1, SHARED_BROTLI_ENCODING);
  table.GetPatchMap().AddEntry({20}, 2, SHARED_BROTLI_ENCODING);
  table.GetPatchMap().AddEntry({30}, 3, SHARED_BROTLI_ENCODING, true);
  table.GetPatchMap().AddEntry({40}, 4, SHARED_BROTLI_ENCODING, true);

  auto font = table.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  auto table_from_font = IFTTable::FromFont(*font);
  ASSERT_TRUE(table_from_font.ok()) << table_from_font.status();

  ASSERT_EQ(table, *table_from_font);
}

TEST_F(IFTTableTest, RoundTrip_ExtraUrlTemplate) {
  IFTTable table;
  table.SetUrlTemplate("files/go/here/$1.br", "extension/files/$1.br");
  table.GetPatchMap().AddEntry({10}, 1, SHARED_BROTLI_ENCODING);
  table.GetPatchMap().AddEntry({20}, 2, SHARED_BROTLI_ENCODING);
  table.GetPatchMap().AddEntry({30}, 3, SHARED_BROTLI_ENCODING, true);
  table.GetPatchMap().AddEntry({40}, 4, SHARED_BROTLI_ENCODING, true);

  auto font = table.AddToFont(roboto_ab.get());
  ASSERT_TRUE(font.ok()) << font.status();

  auto table_from_font = IFTTable::FromFont(*font);
  ASSERT_TRUE(table_from_font.ok()) << table_from_font.status();

  ASSERT_EQ(table, *table_from_font);
}

TEST_F(IFTTableTest, HasExtensionEntries) {
  IFTTable table;
  table.GetPatchMap().AddEntry({10}, 1, SHARED_BROTLI_ENCODING);
  table.GetPatchMap().AddEntry({20}, 2, SHARED_BROTLI_ENCODING);
  ASSERT_FALSE(table.HasExtensionEntries());

  table.GetPatchMap().AddEntry({30}, 3, SHARED_BROTLI_ENCODING, true);
  ASSERT_TRUE(table.HasExtensionEntries());
}

TEST_F(IFTTableTest, AddToFont_IftbConversion) {
  auto font = sample.AddToFont(iftb.get(), true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  hb_blob_get_data(blob.get(), &length);

  EXPECT_GT(length, 1);

  auto expected_tags = FontHelper::GetTags(iftb.get());
  auto new_tags = FontHelper::GetTags(face.get());
  expected_tags.erase(FontHelper::kIFTB);
  expected_tags.insert(FontHelper::kIFT);

  EXPECT_EQ(expected_tags, new_tags);

  auto ordered_tags =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(face.get()));
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 3], "IFT ");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 2], "glyf");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 1], "loca");
}

TEST_F(IFTTableTest, AddToFont_IftbConversionRoboto) {
  auto font = sample.AddToFont(roboto_ab.get(), true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  hb_blob_get_data(blob.get(), &length);
  EXPECT_GT(length, 1);

  auto expected_tags = FontHelper::GetTags(roboto_ab.get());
  auto new_tags = FontHelper::GetTags(face.get());
  expected_tags.insert(FontHelper::kIFT);

  EXPECT_EQ(expected_tags, new_tags);

  auto ordered_tags =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(face.get()));
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 3], "IFT ");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 2], "glyf");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 1], "loca");
}

TEST_F(IFTTableTest, FromFont_Missing) {
  auto table = IFTTable::FromFont(roboto_ab.get());
  ASSERT_FALSE(table.ok()) << table.status();
  ASSERT_TRUE(absl::IsNotFound(table.status()));
}

TEST_F(IFTTableTest, GetId) {
  const uint32_t expected[4] = {1, 2, 3, 4};
  uint32_t actual[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  sample.GetId(actual);

  ASSERT_EQ(expected[0], actual[0]);
  ASSERT_EQ(expected[1], actual[1]);
  ASSERT_EQ(expected[2], actual[2]);
  ASSERT_EQ(expected[3], actual[3]);
}

TEST_F(IFTTableTest, GetId_None) {
  const uint32_t expected[4] = {0, 0, 0, 0};
  uint32_t actual[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  empty.GetId(actual);

  ASSERT_EQ(expected[0], actual[0]);
  ASSERT_EQ(expected[1], actual[1]);
  ASSERT_EQ(expected[2], actual[2]);
  ASSERT_EQ(expected[3], actual[3]);
}

TEST_F(IFTTableTest, SetId_Good) {
  IFTTable table;
  auto s = table.SetId({1, 2, 3, 4});
  ASSERT_TRUE(s.ok()) << s;

  const uint32_t expected[4] = {1, 2, 3, 4};
  uint32_t actual[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  table.GetId(actual);

  ASSERT_EQ(expected[0], actual[0]);
  ASSERT_EQ(expected[1], actual[1]);
  ASSERT_EQ(expected[2], actual[2]);
  ASSERT_EQ(expected[3], actual[3]);
}

TEST_F(IFTTableTest, SetId_Bad) {
  IFTTable table;
  auto s = table.SetId({1, 2, 3, 4, 5});
  ASSERT_TRUE(absl::IsInvalidArgument(s)) << s;
}

}  // namespace ift::proto
