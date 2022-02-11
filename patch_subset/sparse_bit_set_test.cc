#include "patch_subset/sparse_bit_set.h"

#include <bitset>
#include <vector>

#include "common/status.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/hb_set_unique_ptr.h"

using std::bitset;
using std::pair;
using std::string;
using std::to_string;
using std::vector;

namespace patch_subset {

class SparseBitSetTest : public ::testing::Test {
 protected:
  static void TestEncodeDecode(hb_set_unique_ptr set, int expected_size) {
    TestEncodeDecode(set.get(), BF8, expected_size);
  }

  static void TestEncodeDecode(hb_set_unique_ptr set, BranchFactor bf) {
    TestEncodeDecode(set.get(), bf, 0);
  }

  static void TestEncodeDecode(hb_set_t *set, BranchFactor bf,
                               int expected_size) {
    string encoded_bits = SparseBitSet::Encode(*set, bf);
    if (expected_size) {
      EXPECT_EQ(encoded_bits.size(), expected_size);
    }
    hb_set_unique_ptr decoded = make_hb_set();
    StatusCode sc = SparseBitSet::Decode(encoded_bits, decoded.get());
    if (sc != StatusCode::kOk) {
      string bits = Bits(encoded_bits, bf);
      EXPECT_EQ("Decode worked", "Unable to decode bits: " + bits);
    }
    EXPECT_EQ(StatusCode::kOk, sc);
    EXPECT_TRUE(hb_set_is_equal(set, decoded.get()));
  }

  static string Bits(const string &s, BranchFactor bf) {
    if (s.empty()) {
      return "";
    }
    string first8_bits;
    // Decode branch factor and depth from first byte.
    first8_bits += s[0] & 1 ? "1" : "0";
    first8_bits += s[0] & 2 ? "1" : "0";
    first8_bits += "|";
    for (int i = 2; i < 8; i++) {
      first8_bits += s[0] & (1 << i) ? "1" : "0";
    }

    string bits;
    // Decode the rest of the bytes.
    for (unsigned int i = 1; i < s.size(); i++) {
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
    return first8_bits + " " + result;
  }

  static string Bits(hb_set_unique_ptr set, BranchFactor bf) {
    string bits = SparseBitSet::Encode(*set, bf);
    return Bits(bits, bf);
  }

  static string FromChars(const string &s) {
    string result;
    if (s.empty()) {
      return result;
    }
    unsigned char buffer = 0;
    int num_bits = 0;
    for (char c : s) {
      if (c != '1' && c != '0') {
        continue;
      }
      if (c == '1') {
        buffer |= 1 << num_bits;
      }
      num_bits++;
      if (num_bits == 8) {
        result.push_back(buffer);
        buffer = 0;
        num_bits = 0;
      }
    }
    if (num_bits > 0) {
      result.push_back(buffer);
    }
    return result;
  }

  static string FromBits(const string &s) {
    hb_set_unique_ptr set = make_hb_set();
    EXPECT_EQ(StatusCode::kOk, SparseBitSet::Decode(FromChars(s), set.get()));
    std::vector<unsigned int> results;
    for (hb_codepoint_t cp = HB_SET_VALUE_INVALID;
         hb_set_next(set.get(), &cp);) {
      results.push_back(cp);
    }
    string results_str;
    for (unsigned int n : results) {
      results_str += to_string(n);
      results_str += " ";
    }
    if (!results_str.empty()) {
      results_str = results_str.substr(0, results_str.length() - 1);
    }
    return results_str;
  }

  static hb_set_unique_ptr Set(const vector<pair<int, int>> &pairs) {
    hb_set_unique_ptr set = make_hb_set();
    for (pair<int, int> pair : pairs) {
      for (int i = pair.first; i <= pair.second; i++) {
        hb_set_add(set.get(), i);
      }
    }
    return set;
  }
};

TEST_F(SparseBitSetTest, DecodeNullSet) {
  EXPECT_EQ(SparseBitSet::Decode(string(), nullptr),
            StatusCode::kInvalidArgument);
}

TEST_F(SparseBitSetTest, DecodeAppends) {
  hb_set_unique_ptr set = make_hb_set(1, 42);
  SparseBitSet::Decode(string{0b00000001, 0b00000001}, set.get());
  //                          ^ d1 bf8 ^
  hb_set_unique_ptr expected = make_hb_set(2, 0, 42);
  EXPECT_TRUE(hb_set_is_equal(expected.get(), set.get()));
}

TEST_F(SparseBitSetTest, DecodeInvalid) {
  // The encoded set here is truncated and missing 2 bytes.
  string encoded{0b00000101, 0b01010101, 0b00000001, 0b00000001};
  //             ^ d2 bf8 ^
  hb_set_unique_ptr set = make_hb_set();
  EXPECT_EQ(StatusCode::kInvalidArgument,
            SparseBitSet::Decode(encoded, set.get()));

  hb_set_unique_ptr empty_set = make_hb_set(0);
  // The set was partially updated.
  EXPECT_FALSE(hb_set_is_equal(set.get(), empty_set.get()));
}

TEST_F(SparseBitSetTest, EncodeEmpty) { TestEncodeDecode(make_hb_set(), 0); }

TEST_F(SparseBitSetTest, EncodeOneLayer) {
  TestEncodeDecode(make_hb_set(1, 0), 2);
  TestEncodeDecode(make_hb_set(1, 7), 2);
  TestEncodeDecode(make_hb_set(2, 2, 5), 2);
  TestEncodeDecode(make_hb_set(8, 0, 1, 2, 3, 4, 5, 6, 7), 2);
}

TEST_F(SparseBitSetTest, EncodeTwoLayers) {
  TestEncodeDecode(make_hb_set(1, 63), 3);
  TestEncodeDecode(make_hb_set(2, 0, 63), 4);
  TestEncodeDecode(make_hb_set(3, 2, 5, 60), 4);
  TestEncodeDecode(make_hb_set(5, 0, 30, 31, 33, 63), 6);
}

TEST_F(SparseBitSetTest, EncodeManyLayers) {
  TestEncodeDecode(make_hb_set(2, 10, 49596), 12);
  TestEncodeDecode(make_hb_set(3, 10, 49595, 49596), 12);
  TestEncodeDecode(make_hb_set(3, 10, 49588, 49596), 13);
}

TEST_F(SparseBitSetTest, Encode4BitEmpty) {
  EXPECT_EQ("", Bits(make_hb_set(0, 0), BF4));
}

TEST_F(SparseBitSetTest, Encode4BitSingleNode) {
  EXPECT_EQ("00|000000 1000 0000", Bits(make_hb_set(1, 0), BF4));
  //         ^bf4  d1^
  EXPECT_EQ("00|000000 0100 0000", Bits(make_hb_set(1, 1), BF4));
  EXPECT_EQ("00|000000 0010 0000", Bits(make_hb_set(1, 2), BF4));
  EXPECT_EQ("00|000000 0001 0000", Bits(make_hb_set(1, 3), BF4));

  EXPECT_EQ("00|000000 1100 0000", Bits(make_hb_set(2, 0, 1), BF4));
  EXPECT_EQ("00|000000 0011 0000", Bits(make_hb_set(2, 2, 3), BF4));

  EXPECT_EQ("00|000000 1111 0000", Bits(make_hb_set(4, 0, 1, 2, 3), BF4));
}

TEST_F(SparseBitSetTest, Encode4BitMultipleNodes) {
  EXPECT_EQ("00|100000 0100 1000", Bits(make_hb_set(1, 4), BF4));
  EXPECT_EQ("00|100000 0100 0100", Bits(make_hb_set(1, 5), BF4));
  EXPECT_EQ("00|100000 0100 0010", Bits(make_hb_set(1, 6), BF4));
  EXPECT_EQ("00|100000 0100 0001", Bits(make_hb_set(1, 7), BF4));
  //         ^bf4  d1^

  EXPECT_EQ("00|100000 0010 1000", Bits(make_hb_set(1, 8), BF4));
  EXPECT_EQ("00|100000 0010 0100", Bits(make_hb_set(1, 9), BF4));
  EXPECT_EQ("00|100000 0010 0010", Bits(make_hb_set(1, 10), BF4));
  EXPECT_EQ("00|100000 0010 0001", Bits(make_hb_set(1, 11), BF4));

  EXPECT_EQ("00|100000 0001 1000", Bits(make_hb_set(1, 12), BF4));
  EXPECT_EQ("00|100000 0001 0100", Bits(make_hb_set(1, 13), BF4));
  EXPECT_EQ("00|100000 0001 0010", Bits(make_hb_set(1, 14), BF4));
  EXPECT_EQ("00|100000 0001 0001", Bits(make_hb_set(1, 15), BF4));

  EXPECT_EQ("00|100000 1100 1000 1000 0000", Bits(make_hb_set(2, 0, 4), BF4));
  EXPECT_EQ("00|100000 0011 1000 1000 0000", Bits(make_hb_set(2, 8, 12), BF4));
}

TEST_F(SparseBitSetTest, Encode8) {
  EXPECT_EQ(
      "10|010000 "
      // bf8, d3
      "10000100 10001000 10000000 00100000 01000000 00010000",
      Bits(make_hb_set(3, 2, 33, 323), BF8));
}

TEST_F(SparseBitSetTest, LeafNodesNeverFilled) {
  EXPECT_EQ("00|000000 1111 0000", Bits(Set({{0, 3}}), BF4));
  EXPECT_EQ("10|000000 11111111", Bits(Set({{0, 7}}), BF8));
  EXPECT_EQ("01|000000 1111111111111111", Bits(Set({{0, 15}}), BF16));
  EXPECT_EQ("11|000000 11111111111111111111111111111111",
            Bits(Set({{0, 31}}), BF32));
}

TEST_F(SparseBitSetTest, EncodeFilledTwigs) {
  EXPECT_EQ("00|100000 0000 0000", Bits(Set({{0, 15}}), BF4));
  EXPECT_EQ("10|100000 00000000", Bits(Set({{0, 63}}), BF8));
  EXPECT_EQ("01|100000 0000000000000000",
            Bits(Set({{0, (16 * 16) - 1}}), BF16));
  EXPECT_EQ("11|100000 00000000000000000000000000000000",
            Bits(Set({{0, (32 * 32) - 1}}), BF32));
}

TEST_F(SparseBitSetTest, EncodeAllFilledHeight3) {
  EXPECT_EQ("00|010000 0000 0000", Bits(Set({{0, (4 * 4 * 4) - 1}}), BF4));
  EXPECT_EQ("10|010000 00000000", Bits(Set({{0, (8 * 8 * 8) - 1}}), BF8));
  EXPECT_EQ("01|010000 0000000000000000",
            Bits(Set({{0, (16 * 16 * 16) - 1}}), BF16));
  EXPECT_EQ("11|010000 00000000000000000000000000000000",
            Bits(Set({{0, (32 * 32 * 32) - 1}}), BF32));
}

TEST_F(SparseBitSetTest, EncodeAllFilledHeight4) {
  EXPECT_EQ("00|110000 0000 0000", Bits(Set({{0, (4 * 4 * 4 * 4) - 1}}), BF4));
  EXPECT_EQ("10|110000 00000000", Bits(Set({{0, (8 * 8 * 8 * 8) - 1}}), BF8));
  EXPECT_EQ("01|110000 0000000000000000",
            Bits(Set({{0, (16 * 16 * 16 * 16) - 1}}), BF16));
  EXPECT_EQ("11|110000 00000000000000000000000000000000",
            Bits(Set({{0, (32 * 32 * 32 * 32) - 1}}), BF32));
}

TEST_F(SparseBitSetTest, EncodeAllFilledHeight5) {
  EXPECT_EQ("00|001000 0000 0000",
            Bits(Set({{0, (4 * 4 * 4 * 4 * 4) - 1}}), BF4));
  EXPECT_EQ("10|001000 00000000",
            Bits(Set({{0, (8 * 8 * 8 * 8 * 8) - 1}}), BF8));
  EXPECT_EQ("01|001000 0000000000000000",
            Bits(Set({{0, (16 * 16 * 16 * 16 * 16) - 1}}), BF16));
  EXPECT_EQ("11|001000 00000000000000000000000000000000",
            Bits(Set({{0, (32 * 32 * 32 * 32 * 32) - 1}}), BF32));
}

TEST_F(SparseBitSetTest, EncodeComplexFilledNodes) {
  // A 0, then a gap, then a filled node of 16, then a 32.
  EXPECT_EQ("00|010000 1110 1000 0000 1000 1000 1000",
            Bits(Set({{0, 0}, {16, 32}}), BF4));
  // 2 filled nodes of 16, a gap, then a filled leaf (not encoded as 0).
  EXPECT_EQ("00|110000 1100 1100 1000 0000 0000 1000 1111 0000",
            Bits(Set({{0, 31}, {64, 67}}), BF4));
  // 1024 leaf nodes (representing values 0..4095), first 50% are filled.
  EXPECT_EQ("00|101000 1100 0000 0000 0000", Bits(Set({{0, 2047}}), BF4));
}

TEST_F(SparseBitSetTest, Decode4) {
  //                  lsb  ....  msb
  EXPECT_EQ(FromBits("00|000000 1000"), "0");
  EXPECT_EQ(FromBits("00|000000 0100"), "1");
  EXPECT_EQ(FromBits("00|000000 0010"), "2");
  EXPECT_EQ(FromBits("00|000000 0001"), "3");
  EXPECT_EQ(FromBits("00|000000 1111"), "0 1 2 3");
  //                  ^d1  bf4^

  EXPECT_EQ(FromBits("00|100000 1100 1000 1000"), "0 4");
  EXPECT_EQ(FromBits("00|100000 0011 1000 1000"), "8 12");
  //                  ^d2  bf4^
}

TEST_F(SparseBitSetTest, Decode8) {
  EXPECT_EQ(FromBits("10|010000 "
                     // bf8, d3
                     "10000100 10001000 10000000 00100000 01000000 00010000"),
            "2 33 323");
}

TEST_F(SparseBitSetTest, DecodeSimpleFilledTwigNodes) {
  EXPECT_EQ(FromBits("00|100000 0000 0000"),
            "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15");
  EXPECT_EQ(FromBits("10|100000 00000000"),
            "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 "
            "25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 "
            "47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63");
  string s;
  for (int i = 0; i < 16 * 16; i++) {
    s += to_string(i);
    if (i != (16 * 16) - 1) {
      s += " ";
    }
  }
  EXPECT_EQ(FromBits("01|100000 0000000000000000"), s);
  s = "";
  for (int i = 0; i < 32 * 32; i++) {
    s += to_string(i);
    if (i != (32 * 32) - 1) {
      s += " ";
    }
  }
  EXPECT_EQ(FromBits("11|100000 00000000000000000000000000000000"), s);
}

// Encode() does not fill leaves (no space savings), but it is legal.
TEST_F(SparseBitSetTest, DecodeFilledLeaf) {
  EXPECT_EQ(FromBits("00|000000 0000"), "0 1 2 3");
  EXPECT_EQ(FromBits("10|000000 00000000"), "0 1 2 3 4 5 6 7");
  EXPECT_EQ(FromBits("01|000000 0000000000000000"),
            "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15");
  EXPECT_EQ(FromBits("11|000000 00000000000000000000000000000000"),
            "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 "
            "16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31");
}

TEST_F(SparseBitSetTest, MostlyFilledExampleTranscode) {
  string bits =
      SparseBitSet::Encode(*Set({{0, 115}, {117, 217}, {219, 255}}), BF4);
  string bits_str = Bits(bits, BF4);
  EXPECT_EQ(
      "00|110000 "  // BF=4, tree height = 4 (values 0..255).
      // d=0 (root) layer. 4 sub nodes.
      "1111 "
      // d=1 layer. 2 filled (0..64, 128..191), 8 sub nodes.
      "0000 1111 0000 1111 "
      // d=2 layer (twigs). 6 fully filled, 8 sub nodes.
      "0000 0000 0000 1111 0000 1111 0000 0000 "
      // d=3 layer (leaves).
      "1111 0111 1111 1111 1111 1111 1101 1111 0000",
      //    ^ missing 116              ^ missing 218
      //                                       padding
      bits_str);
  hb_set_unique_ptr decoded = make_hb_set();
  EXPECT_EQ(StatusCode::kOk, SparseBitSet::Decode(bits, decoded.get()));
  hb_set_unique_ptr expected = Set({{0, 115}, {117, 217}, {219, 255}});
  EXPECT_TRUE(hb_set_is_equal(expected.get(), decoded.get()));
}

TEST_F(SparseBitSetTest, OneMissingValue4) {
  int max = (BF4 * BF4 * BF4 * BF4) - 1;
  TestEncodeDecode(Set({{1, max}}), BF4);
  for (int i = 1; i < max; i++) {
    TestEncodeDecode(Set({{0, i - 1}, {i + 1, max}}), BF4);
  }
  TestEncodeDecode(Set({{0, max - 1}}), BF4);
}

TEST_F(SparseBitSetTest, OneMissingValue8) {
  int max = (BF8 * BF8 * BF8 * BF8) - 1;
  TestEncodeDecode(Set({{1, max}}), BF8);
  for (int i = 1; i < max; i++) {
    TestEncodeDecode(Set({{0, i - 1}, {i + 1, max}}), BF8);
  }
  TestEncodeDecode(Set({{0, max - 1}}), BF8);
}

TEST_F(SparseBitSetTest, OneMissingValue16) {
  int max = (BF16 * BF16 * BF16 * BF16) - 1;
  TestEncodeDecode(Set({{1, max}}), BF16);
  // Too many values to try all possible combinations.
  for (int i = 1; i < 16 * 18; i += 17) {
    TestEncodeDecode(Set({{0, i - 1}, {i + 1, max}}), BF16);
  }
  for (int i = (max / 2) - 500; i < (max / 2) + 500; i += 17) {
    TestEncodeDecode(Set({{0, i - 1}, {i + 1, max}}), BF16);
  }
  for (int i = max - 16 * 18; i < max; i += 17) {
    TestEncodeDecode(Set({{0, i - 1}, {i + 1, max}}), BF16);
  }
  TestEncodeDecode(Set({{0, max - 1}}), BF16);
}

TEST_F(SparseBitSetTest, OneMissingValue32) {
  int max = (BF32 * BF32 * BF32 * BF32) - 1;
  // Too many values to try all possible combinations.
  TestEncodeDecode(Set({{1, max}}), BF32);
  TestEncodeDecode(Set({{0, (max / 2) - 1}, {(max / 2), max}}), BF32);
  TestEncodeDecode(Set({{0, max - 1}}), BF32);
}

TEST_F(SparseBitSetTest, RandomSets) {
  for (BranchFactor bf : {BF4, BF8, BF16, BF32}) {
    unsigned int seed = 42;
    for (int i = 0; i < 1000; i++) {
      int size = rand_r(&seed) % 6000;
      hb_set_unique_ptr input = make_hb_set();
      for (int j = 0; j < size; j++) {
        hb_set_add(input.get(), rand_r(&seed) % 2048);
      }
      string bit_set = SparseBitSet::Encode(*input, bf);
      hb_set_unique_ptr output = make_hb_set();
      EXPECT_EQ(StatusCode::kOk, SparseBitSet::Decode(bit_set, output.get()));
      EXPECT_TRUE(hb_set_is_equal(input.get(), output.get()));
    }
  }
}

TEST_F(SparseBitSetTest, BranchFactorValues) {
  EXPECT_EQ(4, BF4);
  EXPECT_EQ(8, BF8);
  EXPECT_EQ(16, BF16);
  EXPECT_EQ(32, BF32);
}

TEST_F(SparseBitSetTest, DepthLimits4) {
  hb_set_unique_ptr output = make_hb_set();
  // Depth 16 is OK.
  EXPECT_EQ(
      StatusCode::kOk,
      SparseBitSet::Decode(FromChars("00|111100 00000000"), output.get()));
}

TEST_F(SparseBitSetTest, DepthLimits8) {
  hb_set_unique_ptr output = make_hb_set();
  // Depth 11 is OK.
  EXPECT_EQ(
      StatusCode::kOk,
      SparseBitSet::Decode(FromChars("10|010100 00000000"), output.get()));
  // Depth 12 is too much.
  EXPECT_EQ(
      StatusCode::kInvalidArgument,
      SparseBitSet::Decode(FromChars("10|110100 00000000"), output.get()));
}

TEST_F(SparseBitSetTest, DepthLimits16) {
  hb_set_unique_ptr output = make_hb_set();
  // Depth 9 is OK.
  EXPECT_EQ(StatusCode::kOk,
            SparseBitSet::Decode(FromChars("01|000100 0000000000000000"),
                                 output.get()));
  // Depth 10 is too much.
  EXPECT_EQ(StatusCode::kInvalidArgument,
            SparseBitSet::Decode(FromChars("01|100100 0000000000000000"),
                                 output.get()));
}

TEST_F(SparseBitSetTest, DepthLimits32) {
  hb_set_unique_ptr output = make_hb_set();
  // Depth 7 is OK.
  EXPECT_EQ(StatusCode::kOk,
            SparseBitSet::Decode(
                FromChars("11|011000 00000000000000000000000000000000"),
                output.get()));
  // Depth 8 is too much.
  EXPECT_EQ(StatusCode::kInvalidArgument,
            SparseBitSet::Decode(
                FromChars("11|111000 00000000000000000000000000000000"),
                output.get()));
}

TEST_F(SparseBitSetTest, Entire32BitRange) {
  for (BranchFactor bf : {BF4, BF8, BF16, BF32}) {
    TestEncodeDecode(Set({{0xFFFFFFFF, 0xFFFFFFFF}}), bf);
  }
}

}  // namespace patch_subset
