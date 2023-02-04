#include "patch_subset/cbor/int_utils.h"

#include <vector>

#include "gtest/gtest.h"

namespace patch_subset::cbor {

using absl::Status;
using absl::string_view;
using std::string;
using std::vector;

class IntUtilsTest : public ::testing::Test {};

string encoded_bytes(uint32_t n);
int encoded_size(uint32_t n);
bool encodes_and_decodes(uint32_t n);
bool encodes_and_decodes_vector(const vector<uint32_t>& ints);
bool encode_avoids_buffer_overruns(uint32_t n, size_t req_bytes);

TEST_F(IntUtilsTest, ZigZagEncode) {
  EXPECT_EQ(IntUtils::ZigZagEncode(0), 0);
  EXPECT_EQ(IntUtils::ZigZagEncode(1), 2);
  EXPECT_EQ(IntUtils::ZigZagEncode(2), 4);
  EXPECT_EQ(IntUtils::ZigZagEncode(3), 6);
  EXPECT_EQ(IntUtils::ZigZagEncode(4), 8);

  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MAX - 2), UINT32_MAX - 5);
  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MAX - 1), UINT32_MAX - 3);
  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MAX), UINT32_MAX - 1);

  EXPECT_EQ(IntUtils::ZigZagEncode(-1), 1);
  EXPECT_EQ(IntUtils::ZigZagEncode(-2), 3);
  EXPECT_EQ(IntUtils::ZigZagEncode(-3), 5);
  EXPECT_EQ(IntUtils::ZigZagEncode(-4), 7);

  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MIN + 2), UINT32_MAX - 4);
  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MIN + 1), UINT32_MAX - 2);
  EXPECT_EQ(IntUtils::ZigZagEncode(INT32_MIN), UINT32_MAX);
}

TEST_F(IntUtilsTest, ZigZagDecode) {
  EXPECT_EQ(IntUtils::ZigZagDecode(0), 0);
  EXPECT_EQ(IntUtils::ZigZagDecode(2), 1);
  EXPECT_EQ(IntUtils::ZigZagDecode(4), 2);
  EXPECT_EQ(IntUtils::ZigZagDecode(6), 3);
  EXPECT_EQ(IntUtils::ZigZagDecode(8), 4);

  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX - 5), INT32_MAX - 2);
  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX - 3), INT32_MAX - 1);
  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX - 1), INT32_MAX);

  EXPECT_EQ(IntUtils::ZigZagDecode(1), -1);
  EXPECT_EQ(IntUtils::ZigZagDecode(3), -2);
  EXPECT_EQ(IntUtils::ZigZagDecode(5), -3);
  EXPECT_EQ(IntUtils::ZigZagDecode(7), -4);
  EXPECT_EQ(IntUtils::ZigZagDecode(9), -5);

  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX - 4), INT32_MIN + 2);
  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX - 2), INT32_MIN + 1);
  EXPECT_EQ(IntUtils::ZigZagDecode(UINT32_MAX), INT32_MIN);
}

TEST_F(IntUtilsTest, ZigZagTranscodeBottomUp) {
  for (int64_t i = INT32_MIN; i <= INT32_MAX; i += 1000) {
    ASSERT_EQ(IntUtils::ZigZagDecode(IntUtils::ZigZagEncode(i)), i);
  }
}

TEST_F(IntUtilsTest, ZigZagTranscodeTopDown) {
  for (int64_t i = INT32_MAX; i >= INT32_MIN; i -= 1000) {
    ASSERT_EQ(IntUtils::ZigZagDecode(IntUtils::ZigZagEncode(i)), i);
  }
}

TEST_F(IntUtilsTest, UIntBase128Encode) {
  uint8_t buffer[]{0, 0, 0, 0, 0, 0};
  uint8_t expected[]{0b10000001, 0b00000011, 0, 0, 0, 0};  // 128 + 3 = 131.
  size_t size_in_out = 6;

  Status sc = IntUtils::UIntBase128Encode(131, buffer, &size_in_out);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(size_in_out, 2);
  ASSERT_EQ(strncmp((char*)buffer, (char*)expected, 6), 0);
}

TEST_F(IntUtilsTest, UIntBase128EncodeExamples) {
  EXPECT_EQ(encoded_bytes(0), "00000000000000000000000000000000 -> 00000000");
  EXPECT_EQ(encoded_bytes(1), "00000000000000000000000000000001 -> 00000001");
  EXPECT_EQ(encoded_bytes(127), "00000000000000000000000001111111 -> 01111111");
  EXPECT_EQ(encoded_bytes(128),
            "00000000000000000000000010000000 -> 10000001 00000000");
  EXPECT_EQ(encoded_bytes(255),
            "00000000000000000000000011111111 -> 10000001 01111111");
  EXPECT_EQ(encoded_bytes(16256),
            "00000000000000000011111110000000 -> 11111111 00000000");
  EXPECT_EQ(encoded_bytes(2080768),
            "00000000000111111100000000000000 -> "
            "11111111 10000000 00000000");
  EXPECT_EQ(encoded_bytes(266338304),
            "00001111111000000000000000000000 -> "
            "11111111 10000000 10000000 00000000");
  EXPECT_EQ(encoded_bytes(UINT32_MAX),
            "11111111111111111111111111111111 -> "
            "10001111 11111111 11111111 11111111 01111111");
}

TEST_F(IntUtilsTest, UIntBase128Decode) {
  uint8_t buffer[]{0b10000001, 0b00100100, 0, 0};
  string_view buffer_view((char*)buffer, 4);
  uint32_t n = -1;
  size_t num_bytes;

  Status sc = IntUtils::UIntBase128Decode(buffer_view, &n, &num_bytes);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(n, 164);
  ASSERT_EQ(num_bytes, 2);
}

TEST_F(IntUtilsTest, UIntBase128EncodeSizes) {
  // 7 or less bits.
  for (int i = 0; i < 128; i++) {
    ASSERT_EQ(encoded_size(i), 1);
  }
  // 8 to 14 bits.
  for (int i = 128; i < 16384; i++) {
    ASSERT_EQ(encoded_size(i), 2);
  }
  // 15 to 21 bits.
  for (int i = 16384; i < 2097152; i++) {
    ASSERT_EQ(encoded_size(i), 3);
  }
  // 22 to 28 bits.
  for (int64_t i = 2097152; i < 268435456; i += 1000) {
    ASSERT_EQ(encoded_size(i), 4);
  }
  ASSERT_EQ(encoded_size(268435455), 4);
  // 28 to 32 bits.
  for (int64_t i = 268435456; i < UINT32_MAX; i += 100000) {
    ASSERT_EQ(encoded_size(i), 5);
  }
  ASSERT_EQ(encoded_size(UINT32_MAX), 5);
}

TEST_F(IntUtilsTest, UIntBase128TranscodeBottomUp) {
  for (int64_t i = 0; i <= UINT32_MAX; i += 10000) {
    ASSERT_TRUE(encodes_and_decodes(i));
  }
}

TEST_F(IntUtilsTest, UIntBase128TranscodeTopDown) {
  for (int64_t i = UINT32_MAX; i >= 0; i -= 10000) {
    ASSERT_TRUE(encodes_and_decodes(i));
  }
}

TEST_F(IntUtilsTest, UIntBase128TranscodeLists) {
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{0}));
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{100}));
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{300}));
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{1, 2, 3}));
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{1000, 2000, 3000}));
  EXPECT_TRUE(encodes_and_decodes_vector(
      vector<uint32_t>{268435456, 1, 2000, 654321, 200, 54, 370, 943}));
  EXPECT_TRUE(encodes_and_decodes_vector(vector<uint32_t>{
      268430000, 268431111, 268432222, 268433333, 268434444, 268435555}));
}

TEST_F(IntUtilsTest, UIntBase128BufferSizes) {
  EXPECT_TRUE(encode_avoids_buffer_overruns(0, 1));
  EXPECT_TRUE(encode_avoids_buffer_overruns(1, 1));
  EXPECT_TRUE(encode_avoids_buffer_overruns(50, 1));
  EXPECT_TRUE(encode_avoids_buffer_overruns(100, 1));
  EXPECT_TRUE(encode_avoids_buffer_overruns(127, 1));

  EXPECT_TRUE(encode_avoids_buffer_overruns(128, 2));
  EXPECT_TRUE(encode_avoids_buffer_overruns(1024, 2));
  EXPECT_TRUE(encode_avoids_buffer_overruns(11024, 2));
  EXPECT_TRUE(encode_avoids_buffer_overruns(16383, 2));

  EXPECT_TRUE(encode_avoids_buffer_overruns(16384, 3));
  EXPECT_TRUE(encode_avoids_buffer_overruns(55555, 3));
  EXPECT_TRUE(encode_avoids_buffer_overruns(999999, 3));
  EXPECT_TRUE(encode_avoids_buffer_overruns(2097151, 3));

  EXPECT_TRUE(encode_avoids_buffer_overruns(2097152, 4));
  EXPECT_TRUE(encode_avoids_buffer_overruns(66666666, 4));
  EXPECT_TRUE(encode_avoids_buffer_overruns(111111111, 4));
  EXPECT_TRUE(encode_avoids_buffer_overruns(268435455, 4));

  EXPECT_TRUE(encode_avoids_buffer_overruns(268435456, 5));
  EXPECT_TRUE(encode_avoids_buffer_overruns(3333333333, 5));
  EXPECT_TRUE(encode_avoids_buffer_overruns(UINT32_MAX, 5));
}

TEST_F(IntUtilsTest, IntSizes) {
  // 7 or less bits.
  for (int i = 0; i < 128; i++) {
    ASSERT_EQ(IntUtils::UIntBase128EncodedSize(i), 1);
  }
  // 8 to 14 bits.
  for (int i = 128; i < 16384; i++) {
    ASSERT_EQ(IntUtils::UIntBase128EncodedSize(i), 2);
  }
  // 15 to 21 bits.
  for (int i = 16384; i < 2097152; i++) {
    ASSERT_EQ(IntUtils::UIntBase128EncodedSize(i), 3);
  }
  // 22 to 28 bits.
  for (int64_t i = 2097152; i < 268435456; i += 1000) {
    ASSERT_EQ(IntUtils::UIntBase128EncodedSize(i), 4);
  }
  ASSERT_EQ(IntUtils::UIntBase128EncodedSize(268435455), 4);
  // 28 to 32 bits.
  for (int64_t i = 268435456; i < UINT32_MAX; i += 100000) {
    ASSERT_EQ(IntUtils::UIntBase128EncodedSize(i), 5);
  }
  ASSERT_EQ(IntUtils::UIntBase128EncodedSize(UINT32_MAX), 5);
}

string bits(uint8_t n) {
  string s;
  uint8_t high_bit_mask = 1u << 7;
  for (int i = 0; i < 8; i++) {
    s += n & high_bit_mask ? "1" : "0";
    n <<= 1;
  }
  return s;
}

string bits(uint32_t n) {
  string s;
  uint32_t high_bit_mask = 1u << 31;
  for (int i = 0; i < 32; i++) {
    s += n & high_bit_mask ? "1" : "0";
    n <<= 1;
  }
  return s;
}

string encoded_bytes(uint32_t n) {
  uint8_t buffer[]{0, 0, 0, 0, 0, 0};
  size_t size_in_out = 6;
  IntUtils::UIntBase128Encode(n, buffer, &size_in_out);
  string s = bits(n) + " -> ";
  for (size_t i = 0; i < size_in_out; i++) {
    s += bits(buffer[i]);
    if (i < size_in_out - 1) {
      s += " ";
    }
  }
  return s;
}

int encoded_size(uint32_t n) {
  uint8_t buffer[]{0, 0, 0, 0, 0, 0};
  size_t size_in_out = 6;
  IntUtils::UIntBase128Encode(n, buffer, &size_in_out);
  return (int)size_in_out;
}

bool encodes_and_decodes(uint32_t n) {
  uint8_t buffer[]{0, 0, 0, 0, 0, 0};
  size_t size_in_out = 6;
  Status sc = IntUtils::UIntBase128Encode(n, buffer, &size_in_out);
  if (!sc.ok() || size_in_out == 0) {
    return false;
  }
  string_view buffer_view((char*)buffer, size_in_out);
  uint32_t result;
  sc = IntUtils::UIntBase128Decode(buffer_view, &result, &size_in_out);
  if (!sc.ok() || size_in_out == 0) {
    return false;
  }
  return result == n;
}

bool encodes_and_decodes_vector(const vector<uint32_t>& ints) {
  if (ints.size() * 5 > 2000) {
    return false;  // Might not fit.
  }
  uint8_t buffer[2000];
  uint8_t* next_byte = buffer;
  for (uint32_t n : ints) {
    size_t size_in_out = (buffer + 2000) - next_byte;
    Status sc = IntUtils::UIntBase128Encode(n, next_byte, &size_in_out);
    if (!sc.ok()) {
      return false;
    }
    next_byte += size_in_out;
  }
  size_t final_size = next_byte - buffer;
  vector<uint32_t> result_ints;
  next_byte = buffer;
  for (size_t i = 0; i < ints.size(); i++) {
    uint32_t n;
    size_t num_bytes;
    string_view sv((char*)next_byte, (buffer + final_size) - next_byte);
    Status sc = IntUtils::UIntBase128Decode(sv, &n, &num_bytes);
    if (!sc.ok()) {
      return false;
    }
    result_ints.push_back(n);
    next_byte += num_bytes;
  }
  return result_ints == ints;
}

bool encode_avoids_buffer_overruns(uint32_t n, size_t req_bytes) {
  // All too-small buffers should fail.
  for (int i = 0; i < (int)req_bytes; i++) {
    auto buffer = std::make_unique<uint8_t[]>(std::max(1, i));
    size_t size_in_out = i;
    Status sc = IntUtils::UIntBase128Encode(n, buffer.get(), &size_in_out);
    if (absl::IsInvalidArgument(sc)) {
      return false;
    }
  }
  // All large-enough buffers should work.
  for (size_t i = req_bytes; i < 8; i++) {
    size_t buff_size = i > 0 ? i : 1;
    auto buffer = std::make_unique<uint8_t[]>(buff_size);
    size_t size_in_out = i;
    Status sc = IntUtils::UIntBase128Encode(n, buffer.get(), &size_in_out);
    if (!sc.ok() || size_in_out != req_bytes) {
      return false;
    }
    uint32_t result;
    string_view sv((char*)buffer.get(), size_in_out);
    sc = IntUtils::UIntBase128Decode(sv, &result, &size_in_out);
    if (!sc.ok() || result != n || size_in_out != req_bytes) {
      return false;
    }
  }
  return true;
}
}  // namespace patch_subset::cbor
