#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "util/brotli_bit_buffer.h"

using ::absl::Span;
using ::util::BrotliBitBuffer;

namespace patch_subset {

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

}  // namespace patch_subset
