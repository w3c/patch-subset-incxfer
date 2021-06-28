#include "patch_subset/sparse_bit_set.h"

#include "common/status.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

class SparseBitSetTest : public ::testing::Test {
 protected:
  void TestEncodeDecode(hb_set_unique_ptr set, int expected_size) {
    std::string sparse_bit_set = SparseBitSet::Encode(*set);
    EXPECT_EQ(sparse_bit_set.size(), expected_size);

    hb_set_unique_ptr decoded = make_hb_set();
    EXPECT_EQ(StatusCode::kOk,
              SparseBitSet::Decode(sparse_bit_set, decoded.get()));
    EXPECT_TRUE(hb_set_is_equal(set.get(), decoded.get()));
  }
};

TEST_F(SparseBitSetTest, DecodeNullSet) {
  EXPECT_EQ(SparseBitSet::Decode(std::string(), nullptr),
            StatusCode::kInvalidArgument);
}

TEST_F(SparseBitSetTest, DecodeAppends) {
  hb_set_unique_ptr set = make_hb_set(1, 42);
  SparseBitSet::Decode(std::string{0b00000001}, set.get());

  hb_set_unique_ptr expected = make_hb_set(2, 0, 42);
  EXPECT_TRUE(hb_set_is_equal(expected.get(), set.get()));
}

TEST_F(SparseBitSetTest, EncodeEmpty) { TestEncodeDecode(make_hb_set(), 0); }

TEST_F(SparseBitSetTest, EncodeOneLayer) {
  TestEncodeDecode(make_hb_set(1, 0), 1);
  TestEncodeDecode(make_hb_set(1, 7), 1);
  TestEncodeDecode(make_hb_set(2, 2, 5), 1);
  TestEncodeDecode(make_hb_set(8, 0, 1, 2, 3, 4, 5, 6, 7), 1);
}

TEST_F(SparseBitSetTest, EncodeTwoLayers) {
  TestEncodeDecode(make_hb_set(1, 63), 2);
  TestEncodeDecode(make_hb_set(2, 0, 63), 3);
  TestEncodeDecode(make_hb_set(3, 2, 5, 60), 3);
  TestEncodeDecode(make_hb_set(5, 0, 30, 31, 33, 63), 5);
}

TEST_F(SparseBitSetTest, EncodeManyLayers) {
  TestEncodeDecode(make_hb_set(2, 10, 49596), 11);
  TestEncodeDecode(make_hb_set(3, 10, 49595, 49596), 11);
  TestEncodeDecode(make_hb_set(3, 10, 49588, 49596), 12);
}

TEST_F(SparseBitSetTest, DecodeInvalid) {
  // The encoded set here is truncated and missing 2 bytes.
  std::string encoded{0b01010101, 0b00000001, 0b00000001};
  hb_set_unique_ptr set = make_hb_set();
  EXPECT_EQ(StatusCode::kInvalidArgument,
            SparseBitSet::Decode(encoded, set.get()));

  hb_set_unique_ptr empty_set = make_hb_set(0);
  EXPECT_TRUE(hb_set_is_equal(set.get(), empty_set.get()));
}

}  // namespace patch_subset
