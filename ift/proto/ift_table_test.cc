#include "ift/proto/ift_table.h"

#include <cstdio>
#include <cstring>

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
  IFT complex_ids;
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

TEST_F(IFTTableTest, Mapping_ComplexIds) {
  auto table = IFTTable::FromProto(complex_ids);
  ASSERT_TRUE(table.ok()) << table.status();

  patch_map expected = {
      {0, std::pair(0, SHARED_BROTLI_ENCODING)},
      {2, std::pair(2, SHARED_BROTLI_ENCODING)},
      {4, std::pair(4, SHARED_BROTLI_ENCODING)},
      {5, std::pair(5, SHARED_BROTLI_ENCODING)},
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

TEST_F(IFTTableTest, AddPatch) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->AddPatch({77, 79, 80}, 5, SHARED_BROTLI_ENCODING);
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {30, std::pair(1, SHARED_BROTLI_ENCODING)},
      {32, std::pair(1, SHARED_BROTLI_ENCODING)},
      {55, std::pair(2, IFTB_ENCODING)},
      {56, std::pair(2, IFTB_ENCODING)},
      {57, std::pair(2, IFTB_ENCODING)},
      {77, std::pair(5, SHARED_BROTLI_ENCODING)},
      {79, std::pair(5, SHARED_BROTLI_ENCODING)},
      {80, std::pair(5, SHARED_BROTLI_ENCODING)},
  };

  ASSERT_EQ(table->GetProto().subset_mapping(2).id_delta(), 2);
  ASSERT_GT(table->GetProto().subset_mapping(2).bias(), 0);
  ASSERT_EQ(table->GetProto().subset_mapping(2).patch_encoding(),
            DEFAULT_ENCODING);
  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, AddPatch_NonDefaultEncoding) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->AddPatch({77, 79, 80}, 5, IFTB_ENCODING);
  ASSERT_TRUE(s.ok()) << s;

  ASSERT_GT(table->GetProto().subset_mapping(2).bias(), 0);
  ASSERT_EQ(table->GetProto().subset_mapping(2).patch_encoding(),
            IFTB_ENCODING);
  patch_map expected = {
      {30, std::pair(1, SHARED_BROTLI_ENCODING)},
      {32, std::pair(1, SHARED_BROTLI_ENCODING)},
      {55, std::pair(2, IFTB_ENCODING)},
      {56, std::pair(2, IFTB_ENCODING)},
      {57, std::pair(2, IFTB_ENCODING)},
      {77, std::pair(5, IFTB_ENCODING)},
      {79, std::pair(5, IFTB_ENCODING)},
      {80, std::pair(5, IFTB_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
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

TEST_F(IFTTableTest, RemovePatches_ComplexIds1) {
  auto table = IFTTable::FromProto(complex_ids);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({2});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected1 = {
      {0, std::pair(0, SHARED_BROTLI_ENCODING)},
      {4, std::pair(4, SHARED_BROTLI_ENCODING)},
      {5, std::pair(5, SHARED_BROTLI_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected1);

  s = table->RemovePatches({4});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected2 = {
      {0, std::pair(0, SHARED_BROTLI_ENCODING)},
      {5, std::pair(5, SHARED_BROTLI_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected2);
}

TEST_F(IFTTableTest, RemovePatches_ComplexIds2) {
  auto table = IFTTable::FromProto(complex_ids);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({5});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {0, std::pair(0, SHARED_BROTLI_ENCODING)},
      {2, std::pair(2, SHARED_BROTLI_ENCODING)},
      {4, std::pair(4, SHARED_BROTLI_ENCODING)},
  };

  ASSERT_EQ(table->GetPatchMap(), expected);
}

TEST_F(IFTTableTest, RemovePatches_ComplexIdsMultiple) {
  auto table = IFTTable::FromProto(complex_ids);
  ASSERT_TRUE(table.ok()) << table.status();

  Status s = table->RemovePatches({0, 2});
  ASSERT_TRUE(s.ok()) << s;

  patch_map expected = {
      {4, std::pair(4, SHARED_BROTLI_ENCODING)},
      {5, std::pair(5, SHARED_BROTLI_ENCODING)},
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

TEST_F(IFTTableTest, FromFont) {
  auto font = IFTTable::AddToFont(roboto_ab, sample);
  ASSERT_TRUE(font.ok()) << font.status();

  hb_face_t* face = font->reference_face();
  auto table = IFTTable::FromFont(face);
  hb_face_destroy(face);

  ASSERT_TRUE(table.ok()) << table.status();
  ASSERT_EQ(table->GetUrlTemplate(), "fonts/go/here");
}

TEST_F(IFTTableTest, FromFont_Missing) {
  auto table = IFTTable::FromFont(roboto_ab);
  ASSERT_FALSE(table.ok()) << table.status();
  ASSERT_TRUE(absl::IsNotFound(table.status()));
}

}  // namespace ift::proto
