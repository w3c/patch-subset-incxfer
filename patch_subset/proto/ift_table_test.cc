#include "patch_subset/proto/ift_table.h"

#include <cstring>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using patch_subset::SparseBitSet;

namespace patch_subset::proto {

class IFTTableTest : public ::testing::Test {
 protected:
  IFTTableTest() {
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
  }

  ~IFTTableTest() { hb_face_destroy(roboto_ab); }

  hb_face_t* roboto_ab;
  IFT empty;
  IFT sample;
  IFT overlap_sample;
};

flat_hash_set<uint32_t> get_tags(hb_face_t* face) {
  constexpr uint32_t max_tags = 64;
  hb_tag_t table_tags[max_tags];
  unsigned table_count = max_tags;

  hb_face_get_table_tags(face, 0, &table_count, table_tags);

  flat_hash_set<uint32_t> tag_set;
  for (unsigned i = 0; i < table_count; i++) {
    tag_set.insert(table_tags[i]);
  }

  return tag_set;
}

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

  flat_hash_set<uint32_t> table_tags = get_tags(face);
  flat_hash_set<uint32_t> expected_table_tags = get_tags(roboto_ab);
  hb_face_destroy(face);

  table_tags.erase(HB_TAG('I', 'F', 'T', ' '));

  EXPECT_EQ(expected_table_tags, table_tags);
}

TEST_F(IFTTableTest, Empty) {
  auto table = IFTTable::FromProto(empty);
  ASSERT_TRUE(table.ok()) << table.status();
  flat_hash_map<uint32_t, uint32_t> expected = {};
  ASSERT_EQ(table->get_patch_map(), expected);
}

TEST_F(IFTTableTest, Mapping) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  flat_hash_map<uint32_t, uint32_t> expected = {
      {30, 1}, {32, 1}, {55, 2}, {56, 2}, {57, 2},
  };

  ASSERT_EQ(table->get_patch_map(), expected);
}

TEST_F(IFTTableTest, OverlapFails) {
  auto table = IFTTable::FromProto(overlap_sample);
  ASSERT_TRUE(absl::IsInvalidArgument(table.status())) << table.status();
}

}  // namespace patch_subset::proto
