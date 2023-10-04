#include "ift/proto/ift_table.h"

#include <cstdio>
#include <cstring>
#include <iterator>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using common::FontHelper;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using patch_subset::SparseBitSet;

namespace ift::proto {

class IFTTableTest : public ::testing::Test {
 protected:
  IFTTableTest() {
    sample.set_default_patch_encoding(SHARED_BROTLI_ENCODING);

    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(1);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(2);
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
    m->set_id(3);

    hb_blob_t* blob =
        hb_blob_create_from_file("patch_subset/testdata/Roboto-Regular.ab.ttf");
    roboto_ab = hb_face_create(blob, 0);
    hb_blob_destroy(blob);

    blob = hb_blob_create_from_file("ift/testdata/NotoSansJP-Regular.iftb.ttf");
    FontData font_data(blob);
    hb_blob_destroy(blob);

    std::string copy = font_data.string();
    copy[0] = 'O';
    copy[1] = 'T';
    copy[2] = 'T';
    copy[3] = 'O';

    font_data.copy(copy);
    iftb = font_data.reference_face();
  }

  ~IFTTableTest() { hb_face_destroy(roboto_ab); }

  hb_face_t* roboto_ab;
  hb_face_t* iftb;
  IFT empty;
  IFT sample;
  IFT overlap_sample;
  IFT good_id;
  IFT bad_id;
};

TEST_F(IFTTableTest, AddToFont) {
  auto font = IFTTable::AddToFont(roboto_ab, sample);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_t* face = font->reference_face();

  hb_blob_t* blob = hb_face_reference_table(face, HB_TAG('I', 'F', 'T', ' '));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob, &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

  hb_blob_destroy(blob);

  auto original_tag_order =
      FontHelper::ToStrings(FontHelper::GetOrderedTags(roboto_ab));
  auto new_tag_order = FontHelper::ToStrings(FontHelper::GetOrderedTags(face));
  hb_face_destroy(face);

  new_tag_order.erase(
      std::find(new_tag_order.begin(), new_tag_order.end(), "IFT "));

  EXPECT_EQ(original_tag_order, new_tag_order);
}

TEST_F(IFTTableTest, AddToFont_IftbConversion) {
  auto font = IFTTable::AddToFont(iftb, sample, true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_t* face = font->reference_face();
  hb_blob_t* blob = hb_face_reference_table(face, HB_TAG('I', 'F', 'T', ' '));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob, &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

  hb_blob_destroy(blob);

  auto expected_tags = FontHelper::GetTags(iftb);
  auto new_tags = FontHelper::GetTags(face);
  expected_tags.erase(FontHelper::kIFTB);
  expected_tags.insert(FontHelper::kIFT);

  EXPECT_EQ(expected_tags, new_tags);

  auto ordered_tags = FontHelper::ToStrings(FontHelper::GetOrderedTags(face));
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 3], "IFT ");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 2], "glyf");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 1], "loca");
}

TEST_F(IFTTableTest, AddToFont_IftbConversionRoboto) {
  auto font = IFTTable::AddToFont(roboto_ab, sample, true);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_t* face = font->reference_face();
  hb_blob_t* blob = hb_face_reference_table(face, HB_TAG('I', 'F', 'T', ' '));

  unsigned length = 0;
  const char* data = hb_blob_get_data(blob, &length);

  std::string expected = sample.SerializeAsString();
  EXPECT_EQ(expected.size(), length);
  if (expected.size() == length) {
    EXPECT_EQ(memcmp(expected.data(), data, length), 0);
  }

  hb_blob_destroy(blob);

  auto expected_tags = FontHelper::GetTags(roboto_ab);
  auto new_tags = FontHelper::GetTags(face);
  expected_tags.insert(FontHelper::kIFT);

  EXPECT_EQ(expected_tags, new_tags);

  auto ordered_tags = FontHelper::ToStrings(FontHelper::GetOrderedTags(face));
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 3], "IFT ");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 2], "glyf");
  EXPECT_EQ(ordered_tags[ordered_tags.size() - 1], "loca");
}

TEST_F(IFTTableTest, Empty) {
  auto table = IFTTable::FromProto(empty);
  ASSERT_TRUE(table.ok()) << table.status();
  patch_map expected = {};
  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, Mapping) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  patch_map expected = {
      {30, std::pair(1, SHARED_BROTLI_ENCODING)},
      {32, std::pair(1, SHARED_BROTLI_ENCODING)},
      {55, std::pair(2, IFTB_ENCODING)},
      {56, std::pair(2, IFTB_ENCODING)},
      {57, std::pair(2, IFTB_ENCODING)},
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

TEST_F(IFTTableTest, OverlapFails) {
  auto table = IFTTable::FromProto(overlap_sample);
  ASSERT_TRUE(absl::IsInvalidArgument(table.status())) << table.status();
}

// format at end of string

TEST_F(IFTTableTest, PatchToUrl_NoFormatters) {
  IFT ift;
  ift.set_url_template("https://localhost/abc.patch");
  auto table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/abc.patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/abc.patch");
}

TEST_F(IFTTableTest, PatchToUrl_InvalidFormatter) {
  IFT ift;
  ift.set_url_template("https://localhost/$1.$patch");
  auto table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/0.$patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/5.$patch");

  ift.set_url_template("https://localhost/$1.patch$");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/0.patch$");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/5.patch$");

  ift.set_url_template("https://localhost/$1.pa$$2tch");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/0.pa$0tch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/5.pa$0tch");
  EXPECT_EQ(table->PatchToUrl(18), "https://localhost/2.pa$1tch");

  ift.set_url_template("https://localhost/$6.patch");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/$6.patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/$6.patch");

  ift.set_url_template("https://localhost/$12.patch");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/02.patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/52.patch");
}

TEST_F(IFTTableTest, PatchToUrl_Basic) {
  IFT ift;
  ift.set_url_template("https://localhost/$2$1.patch");
  auto table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/00.patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/05.patch");
  EXPECT_EQ(table->PatchToUrl(12), "https://localhost/0c.patch");
  EXPECT_EQ(table->PatchToUrl(18), "https://localhost/12.patch");
  EXPECT_EQ(table->PatchToUrl(212), "https://localhost/d4.patch");

  ift.set_url_template("https://localhost/$2$1");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/00");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/05");
  EXPECT_EQ(table->PatchToUrl(12), "https://localhost/0c");
  EXPECT_EQ(table->PatchToUrl(18), "https://localhost/12");
  EXPECT_EQ(table->PatchToUrl(212), "https://localhost/d4");

  ift.set_url_template("$2$1.patch");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "00.patch");
  EXPECT_EQ(table->PatchToUrl(5), "05.patch");
  EXPECT_EQ(table->PatchToUrl(12), "0c.patch");
  EXPECT_EQ(table->PatchToUrl(18), "12.patch");
  EXPECT_EQ(table->PatchToUrl(212), "d4.patch");

  ift.set_url_template("$1");
  table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "0");
  EXPECT_EQ(table->PatchToUrl(5), "5");
}

TEST_F(IFTTableTest, PatchToUrl_Complex) {
  IFT ift;
  ift.set_url_template("https://localhost/$5/$3/$3$2$1.patch");
  auto table = IFTTable::FromProto(ift);
  ASSERT_TRUE(table.ok()) << table.status();

  EXPECT_EQ(table->PatchToUrl(0), "https://localhost/0/0/000.patch");
  EXPECT_EQ(table->PatchToUrl(5), "https://localhost/0/0/005.patch");
  EXPECT_EQ(table->PatchToUrl(200000), "https://localhost/3/d/d40.patch");
}

TEST_F(IFTTableTest, RemovePatches) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({1});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {55, std::pair(2, IFTB_ENCODING)},
      {56, std::pair(2, IFTB_ENCODING)},
      {57, std::pair(2, IFTB_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, RemovePatches_None) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {30, std::pair(1, SHARED_BROTLI_ENCODING)},
      {32, std::pair(1, SHARED_BROTLI_ENCODING)},
      {55, std::pair(2, IFTB_ENCODING)},
      {56, std::pair(2, IFTB_ENCODING)},
      {57, std::pair(2, IFTB_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, RemovePatches_All) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({1, 2});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {};

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, RemovePatches_BadIds) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({42, 2});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {30, std::pair(1, SHARED_BROTLI_ENCODING)},
      {32, std::pair(1, SHARED_BROTLI_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

}  // namespace ift::proto
