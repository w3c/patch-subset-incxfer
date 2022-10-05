#include "brotli/brotli_bit_buffer.h"

#include <vector>
#include "absl/types/span.h"

namespace brotli {

void BrotliBitBuffer::append_number(uint32_t bits, unsigned count) {

  count = std::min(32u, count);
  unsigned written = 0;

  while (written < count) {
    if (bit_index_ == 8) {
      bit_index_ = 0;
      buffer_.push_back(0);
    }

    unsigned space = 8 - bit_index_;
    unsigned to_write_count = std::min(space, count - written);
    uint8_t mask = (uint8_t)
                   (bits << bit_index_) &
                   ((1 << (to_write_count + bit_index_)) - 1);
    buffer_[buffer_.size() - 1] |= mask;

    written += to_write_count;
    bit_index_ += to_write_count;
    bits = bits >> to_write_count;
  }
}

static uint8_t lookup[16] = {
  0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
  0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

uint8_t reverse(uint8_t value) {
   return (lookup[value & 0b1111] << 4) | lookup[value >> 4];
}

void BrotliBitBuffer::append_prefix_code(uint8_t bits, unsigned count) {
  count = std::min(8u, count);
  // Prefix codes are ordered from MSB to LSB instead of the usual LSB to MSB
  // so reverse the order before appending.
  append_number(reverse(bits) >> (8 - count), count);
}

}  // namespace brotli
