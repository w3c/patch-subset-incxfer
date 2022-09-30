#include "util/brotli_bit_buffer.h"

#include <vector>
#include "absl/types/span.h"

namespace util {

void BrotliBitBuffer::append_number(uint32_t bits, unsigned count) {

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

void BrotliBitBuffer::append_prefix_code(uint32_t bits, unsigned count) {
  // TODO
}

absl::Span<const uint8_t> BrotliBitBuffer::data() const {
  return absl::Span<const uint8_t>(buffer_);
}

}  // namespace util
