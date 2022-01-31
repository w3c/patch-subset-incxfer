#include "patch_subset/bit_output_buffer.h"

#include <bitset>
#include <string>

#include "gtest/gtest.h"

namespace patch_subset {

using std::bitset;
using std::string;

class BitOutputBufferTest : public ::testing::Test {
 protected:
 protected:
  static string Bits(const string &s) {
    string result;
    for (unsigned int i = 0; i < s.size(); i++) {
      string byte_bits = bitset<8>(s.c_str()[i]).to_string();
      reverse(byte_bits.begin(), byte_bits.end());
      result += byte_bits;
    }
    return result;
  }
};

TEST_F(BitOutputBufferTest, SingleWrite4) {
  BitOutputBuffer buf(BranchFactor::BF4);
  buf.append(0b111111111111u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("11110000", out_bits);
}

TEST_F(BitOutputBufferTest, MultipleWrites4) {
  BitOutputBuffer buf(BranchFactor::BF4);
  buf.append(0b1001u);
  buf.append(0b0110u);
  buf.append(0b1100u);
  buf.append(0b0011u);
  buf.append(0b0101u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("100101100011110010100000", out_bits);
}

TEST_F(BitOutputBufferTest, SingleWrite8) {
  BitOutputBuffer buf(BranchFactor::BF8);
  buf.append(0b11111111111111111111u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("11111111", out_bits);
}

TEST_F(BitOutputBufferTest, MultipleWrites8) {
  BitOutputBuffer buf(BranchFactor::BF8);
  buf.append(0b11111111u);
  buf.append(0b00000000u);
  buf.append(0b11110000u);
  buf.append(0b00001111u);
  buf.append(0b10101010u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("1111111100000000000011111111000001010101", out_bits);
}

TEST_F(BitOutputBufferTest, SingleWrite16) {
  BitOutputBuffer buf(BranchFactor::BF16);
  buf.append(0b11111111111111111111111111111111u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("1111111111111111", out_bits);
}

TEST_F(BitOutputBufferTest, MultipleWrites16) {
  BitOutputBuffer buf(BranchFactor::BF16);
  buf.append(0b1111111111111111u);
  buf.append(0b0000000000000000u);
  buf.append(0b1111111100000000u);
  buf.append(0b0000000011111111u);
  buf.append(0b1110001100101100u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ(
      "1111111111111111"
      "0000000000000000"
      "0000000011111111"
      "1111111100000000"
      "0011010011000111",
      out_bits);
}

TEST_F(BitOutputBufferTest, SingleWrite32) {
  BitOutputBuffer buf(BranchFactor::BF32);
  buf.append(0b11111111111111111111111111111111u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ("11111111111111111111111111111111", out_bits);
}

TEST_F(BitOutputBufferTest, MultipleWrites32) {
  BitOutputBuffer buf(BranchFactor::BF32);
  buf.append(0b11111111111111111111111111111111u);
  buf.append(0b00000000000000000000000000000000u);
  buf.append(0b11111111111111110000000000000000u);
  buf.append(0b11111111000000001111111100000000u);
  buf.append(0b11110000111000110010110000000000u);
  std::string out_bits = Bits(buf.to_string());
  EXPECT_EQ(
      "11111111111111111111111111111111"
      "00000000000000000000000000000000"
      "00000000000000001111111111111111"
      "00000000111111110000000011111111"
      "00000000001101001100011100001111",
      out_bits);
}

TEST_F(BitOutputBufferTest, EmptyBuffer) {
  EXPECT_EQ(BitOutputBuffer(BranchFactor::BF4).to_string(), "");
  EXPECT_EQ(BitOutputBuffer(BranchFactor::BF8).to_string(), "");
  EXPECT_EQ(BitOutputBuffer(BranchFactor::BF16).to_string(), "");
  EXPECT_EQ(BitOutputBuffer(BranchFactor::BF32).to_string(), "");
}
}  // namespace patch_subset
