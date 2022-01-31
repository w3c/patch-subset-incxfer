#include "patch_subset/sparse_bit_set.h"

#include <bitset>

#include "common/status.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/hb_set_unique_ptr.h"

using std::bitset;
using std::string;

namespace patch_subset {

class SparseBitSetTest : public ::testing::Test {
 protected:
  static void TestEncodeDecode(hb_set_unique_ptr set, int expected_size) {
    std::string sparse_bit_set = SparseBitSet::Encode(*set, BF8);
    EXPECT_EQ(sparse_bit_set.size(), expected_size);

    hb_set_unique_ptr decoded = make_hb_set();
    EXPECT_EQ(StatusCode::kOk,
              SparseBitSet::Decode(sparse_bit_set, decoded.get()));
    EXPECT_TRUE(hb_set_is_equal(set.get(), decoded.get()));
  }

  static string Bits(const string &s, BranchFactor bf) {
    string bits;
    for (unsigned int i = 0; i < s.size(); i++) {
      string byte_bits = bitset<8>(s.c_str()[i]).to_string();
      reverse(byte_bits.begin(), byte_bits.end());
      bits += byte_bits;
    }
    string result;
    while (!bits.empty()) {
      int n = bits.size() < bf ? (int)bits.size() : bf;
      result += bits.substr(0, n);
      bits = bits.substr(n, bits.size());
      if (!bits.empty()) {
        result += " ";
      }
    }
    return result;
  }

  static string Bits(hb_set_unique_ptr set, BranchFactor bf) {
    return Bits(SparseBitSet::Encode(*set, bf), bf);
  }
};

TEST_F(SparseBitSetTest, DecodeNullSet) {
  EXPECT_EQ(SparseBitSet::Decode(string(), nullptr),
            StatusCode::kInvalidArgument);
}

TEST_F(SparseBitSetTest, DecodeAppends) {
  hb_set_unique_ptr set = make_hb_set(1, 42);
  SparseBitSet::Decode(string{0b00000001}, set.get());

  hb_set_unique_ptr expected = make_hb_set(2, 0, 42);
  EXPECT_TRUE(hb_set_is_equal(expected.get(), set.get()));
}

TEST_F(SparseBitSetTest, DecodeInvalid) {
  // The encoded set here is truncated and missing 2 bytes.
  string encoded{0b01010101, 0b00000001, 0b00000001};
  hb_set_unique_ptr set = make_hb_set();
  EXPECT_EQ(StatusCode::kInvalidArgument,
            SparseBitSet::Decode(encoded, set.get()));

  hb_set_unique_ptr empty_set = make_hb_set(0);
  EXPECT_TRUE(hb_set_is_equal(set.get(), empty_set.get()));
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

TEST_F(SparseBitSetTest, Encode4BitEmpty) {
  EXPECT_EQ("", Bits(make_hb_set(0, 0), BF4));
}

TEST_F(SparseBitSetTest, Encode4BitSingleNode) {
  EXPECT_EQ("1000 0000", Bits(make_hb_set(1, 0), BF4));
  EXPECT_EQ("0100 0000", Bits(make_hb_set(1, 1), BF4));
  EXPECT_EQ("0010 0000", Bits(make_hb_set(1, 2), BF4));
  EXPECT_EQ("0001 0000", Bits(make_hb_set(1, 3), BF4));

  EXPECT_EQ("1100 0000", Bits(make_hb_set(2, 0, 1), BF4));
  EXPECT_EQ("0011 0000", Bits(make_hb_set(2, 2, 3), BF4));

  EXPECT_EQ("1111 0000", Bits(make_hb_set(4, 0, 1, 2, 3), BF4));
}

TEST_F(SparseBitSetTest, Encode4BitMultipleNodes) {
  EXPECT_EQ("0100 1000", Bits(make_hb_set(1, 4), BF4));
  EXPECT_EQ("0100 0100", Bits(make_hb_set(1, 5), BF4));
  EXPECT_EQ("0100 0010", Bits(make_hb_set(1, 6), BF4));
  EXPECT_EQ("0100 0001", Bits(make_hb_set(1, 7), BF4));

  EXPECT_EQ("0010 1000", Bits(make_hb_set(1, 8), BF4));
  EXPECT_EQ("0010 0100", Bits(make_hb_set(1, 9), BF4));
  EXPECT_EQ("0010 0010", Bits(make_hb_set(1, 10), BF4));
  EXPECT_EQ("0010 0001", Bits(make_hb_set(1, 11), BF4));

  EXPECT_EQ("0001 1000", Bits(make_hb_set(1, 12), BF4));
  EXPECT_EQ("0001 0100", Bits(make_hb_set(1, 13), BF4));
  EXPECT_EQ("0001 0010", Bits(make_hb_set(1, 14), BF4));
  EXPECT_EQ("0001 0001", Bits(make_hb_set(1, 15), BF4));

  EXPECT_EQ("1100 1000 1000 0000", Bits(make_hb_set(2, 0, 4), BF4));
  EXPECT_EQ("0011 1000 1000 0000", Bits(make_hb_set(2, 8, 12), BF4));

  EXPECT_EQ("1111 1111 1111 1111 1111 0000",
            Bits(make_hb_set(16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                             14, 15),
                 BF4));
}

TEST_F(SparseBitSetTest, RandomSets) {
  for (BranchFactor bf : {BF4, BF8, BF16, BF32}) {
    if (bf != BF8) {
      continue;  // TODO(andyj): remove this in the next CL with Decode().
    }
    unsigned int seed = 42;
    for (int i = 0; i < 10000; i++) {
      int size = rand_r(&seed) % 4096;
      hb_set_unique_ptr input = make_hb_set();
      for (int j = 0; j < size; j++) {
        hb_set_add(input.get(), rand_r(&seed) % 65535);
      }
      string bit_set = SparseBitSet::Encode(*input, bf);
      hb_set_unique_ptr output = make_hb_set();
      SparseBitSet::Decode(bit_set, output.get());
      EXPECT_EQ(hb_set_is_equal(input.get(), output.get()), true);
    }
  }
}

}  // namespace patch_subset
