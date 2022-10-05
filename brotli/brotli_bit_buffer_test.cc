#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "brotli/brotli_bit_buffer.h"

using ::absl::Span;

namespace brotli {

class BrotliBitBufferTest : public ::testing::Test {
 protected:
  BrotliBitBufferTest() {}

  ~BrotliBitBufferTest() override {}

  void SetUp() override {
  }
};

TEST_F(BrotliBitBufferTest, Append) {
  BrotliBitBuffer buffer;

  buffer.append_number(123, 0);
  EXPECT_EQ(Span<const uint8_t> ({}), buffer.data());

  buffer.append_number(123, 8);
  EXPECT_EQ(Span<const uint8_t> ({123}), buffer.data());

  buffer.append_number(0b10001010, 4);
  EXPECT_EQ(Span<const uint8_t> ({123, 0b00001010}),  buffer.data());

  buffer.append_number(0b01001011, 7);
  EXPECT_EQ(Span<const uint8_t> ({123, 0b10111010, 0b00000100}),  buffer.data());

  buffer.append_number(0b00100000100001000100101, 23);
  EXPECT_EQ(Span<const uint8_t> ({
        123,
        0b10111010,
        0b00101100,
        0b00010001,
        0b10000010,
        0b00000000,
      }),  buffer.data());
}

TEST_F(BrotliBitBufferTest, AppendPrefix) {
  BrotliBitBuffer buffer;

  buffer.append_prefix_code(0b1, 1);
  EXPECT_EQ(Span<const uint8_t> ({0b00000001}), buffer.data());

  buffer.append_prefix_code(0b11010, 5);
  EXPECT_EQ(Span<const uint8_t> ({0b00010111}), buffer.data());
}

TEST_F(BrotliBitBufferTest, AppendOutOfBounds) {
  BrotliBitBuffer buffer;

  buffer.append_number(0x0D0C0B0A, 48);
  EXPECT_EQ(Span<const uint8_t> ({0x0A, 0x0B, 0x0C, 0x0D}), buffer.data());

  buffer.append_prefix_code(0b11001100, 48);
  EXPECT_EQ(Span<const uint8_t> ({0x0A, 0x0B, 0x0C, 0x0D, 0b00110011}), buffer.data());


}

}  // namespace brotli
