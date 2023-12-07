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
    sample.set_url_template("fonts/go/here");
    sample.set_default_patch_encoding(SHARED_BROTLI_ENCODING);

    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);
    m->set_patch_encoding(IFTB_ENCODING);

    good_id = sample;
    good_id.add_id(1);
    good_id.add_id(2);
    good_id.add_id(3);
    good_id.add_id(4);

    bad_id = good_id;
    bad_id.add_id(5);

    overlap_sample = sample;

    m = overlap_sample.add_subset_mapping();
    set = make_hb_set(1, 55);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);

    complex_ids.set_url_template("fonts/go/here");
    complex_ids.set_default_patch_encoding(SHARED_BROTLI_ENCODING);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 0);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(-1);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 5);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(4);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 2);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(-4);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 4);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(1);

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
  IFT empty;
  IFT sample;
  IFT overlap_sample;
  IFT complex_ids;
  IFT good_id;
  IFT bad_id;
};

TEST_F(IFTTableTest, AddToFont) {
  auto font = IFTTable::AddToFont(roboto_ab.get(), sample);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob.get(), &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

  auto original_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(roboto_ab.get()));
  auto new_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(face.get()));

  new_tag_order.erase(
      std::find(new_tag_order.begin(), new_tag_order.end(), "IFT "));

  EXPECT_EQ(original_tag_order, new_tag_order);
}

TEST_F(IFTTableTest, AddToFont_WithExtension) {
  auto font = IFTTable::AddToFont(roboto_ab.get(), sample, &complex_ids);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();

  FontData ift_table =
      FontHelper::TableData(face.get(), HB_TAG('I', 'F', 'T', ' '));
  FontData iftx_table =
      FontHelper::TableData(face.get(), HB_TAG('I', 'F', 'T', 'X'));

  std::string expected_ift = sample.SerializeAsString();
  ASSERT_EQ(expected_ift.size(), ift_table.size());
  ASSERT_EQ(memcmp(expected_ift.data(), ift_table.data(), ift_table.size()), 0);

  std::string expected_iftx = complex_ids.SerializeAsString();
  ASSERT_EQ(expected_iftx.size(), iftx_table.size());
  ASSERT_EQ(memcmp(expected_iftx.data(), iftx_table.data(), iftx_table.size()),
            0);

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

  PatchMap expected = {
      {{10}, 1, SHARED_BROTLI_ENCODING},
      {{20}, 2, SHARED_BROTLI_ENCODING},
      {{30}, 3, SHARED_BROTLI_ENCODING, true},
      {{40}, 4, SHARED_BROTLI_ENCODING, true},
  };
  ASSERT_EQ(expected, table_from_font->GetPatchMap());
  ASSERT_EQ(table_from_font->GetUrlTemplate(), "files/go/here/$1.br");
  ASSERT_EQ(table_from_font->GetExtensionUrlTemplate(), "files/go/here/$1.br");
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

  ASSERT_EQ(table_from_font->GetUrlTemplate(), "files/go/here/$1.br");
  ASSERT_EQ(table_from_font->GetExtensionUrlTemplate(),
            "extension/files/$1.br");
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
  auto font = IFTTable::AddToFont(iftb.get(), sample, nullptr, true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob.get(), &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

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
  auto font = IFTTable::AddToFont(roboto_ab.get(), sample, nullptr, true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  hb_blob_unique_ptr blob = make_hb_blob(
      hb_face_reference_table(face.get(), HB_TAG('I', 'F', 'T', ' ')));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob.get(), &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

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

TEST_F(IFTTableTest, Empty) {
  auto table = IFTTable::FromProto(empty);
  ASSERT_TRUE(table.ok()) << table.status();
  PatchMap expected = {};
  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, Mapping) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  PatchMap expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, Mapping_ComplexIds) {
  auto table = IFTTable::FromProto(complex_ids);
  ASSERT_TRUE(table.ok()) << table.status();

  PatchMap expected = {
      {{0}, 0, SHARED_BROTLI_ENCODING},
      {{5}, 5, SHARED_BROTLI_ENCODING},
      {{2}, 2, SHARED_BROTLI_ENCODING},
      {{4}, 4, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, GetId_None) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  const uint32_t expected[4] = {0, 0, 0, 0};
  uint32_t actual[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  table->GetId(actual);

  ASSERT_EQ(expected[0], actual[0]);
  ASSERT_EQ(expected[1], actual[1]);
  ASSERT_EQ(expected[2], actual[2]);
  ASSERT_EQ(expected[3], actual[3]);
}

TEST_F(IFTTableTest, GetId_Good) {
  auto table = IFTTable::FromProto(good_id);
  ASSERT_TRUE(table.ok()) << table.status();

  const uint32_t expected[4] = {1, 2, 3, 4};
  uint32_t actual[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  table->GetId(actual);

  ASSERT_EQ(expected[0], actual[0]);
  ASSERT_EQ(expected[1], actual[1]);
  ASSERT_EQ(expected[2], actual[2]);
  ASSERT_EQ(expected[3], actual[3]);
}

TEST_F(IFTTableTest, GetId_Bad) {
  auto table = IFTTable::FromProto(bad_id);
  ASSERT_TRUE(absl::IsInvalidArgument(table.status())) << table.status();
}

TEST_F(IFTTableTest, OverlapSucceeds) {
  auto table = IFTTable::FromProto(overlap_sample);
  ASSERT_TRUE(table.ok()) << table.status();
}

TEST_F(IFTTableTest, FromFont) {
  auto font = IFTTable::AddToFont(roboto_ab.get(), sample);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  auto table = IFTTable::FromFont(face.get());

  ASSERT_TRUE(table.ok()) << table.status();
  ASSERT_EQ(table->GetUrlTemplate(), "fonts/go/here");
}

TEST_F(IFTTableTest, FromFont_WithExtension) {
  auto font = IFTTable::AddToFont(roboto_ab.get(), sample, &complex_ids);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_unique_ptr face = font->face();
  auto table = IFTTable::FromFont(face.get());

  ASSERT_TRUE(table.ok()) << table.status();
  ASSERT_EQ(table->GetUrlTemplate(), "fonts/go/here");

  PatchMap expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
      {{0}, 0, SHARED_BROTLI_ENCODING, true},
      {{5}, 5, SHARED_BROTLI_ENCODING, true},
      {{2}, 2, SHARED_BROTLI_ENCODING, true},
      {{4}, 4, SHARED_BROTLI_ENCODING, true},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, FromFont_Missing) {
  auto table = IFTTable::FromFont(roboto_ab.get());
  ASSERT_FALSE(table.ok()) << table.status();
  ASSERT_TRUE(absl::IsNotFound(table.status()));
}

}  // namespace ift::proto
