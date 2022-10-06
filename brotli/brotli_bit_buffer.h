#ifndef BROTLI_BROTLI_BIT_BUFFER_H_
#define BROTLI_BROTLI_BIT_BUFFER_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"

namespace brotli {

/*
 * A class to help write out a brotli byte stream which is a concatenation
 * of multi-bit values. Follows the bit packing conventions from:
 * https://datatracker.ietf.org/doc/html/rfc7932#section-1.5.1
 */
class BrotliBitBuffer {
 public:
  BrotliBitBuffer() : buffer_(), bit_index_(8) {}

  // Regular numbers are appened from LSB to MSB.
  void append_number(uint32_t bits, unsigned count);

  // Prefix codes are appened from MSB to LSB
  void append_prefix_code(uint8_t bits, unsigned count);

  void append_raw(absl::Span<const uint8_t> bytes) {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
  }

  void pad_to_end_of_byte() { bit_index_ = 8; }

  bool is_byte_aligned() const { return bit_index_ == 8; }

  absl::Span<const uint8_t> data() const {
    return absl::Span<const uint8_t>(buffer_);
  }

  std::vector<uint8_t>& sink() { return buffer_; }

 private:
  std::vector<uint8_t> buffer_;
  // index of the next bit to be written in the current byte,
  // value is in [0-7]
  unsigned bit_index_;
};

}  // namespace brotli

#endif  // BROTLI_BROTLI_BIT_BUFFER_H_
