#ifndef UTIL_BROTLI_BIT_BUFFER_H_
#define UTIL_BROTLI_BIT_BUFFER_H_

#include <cstdint>
#include <vector>
#include "absl/types/span.h"

namespace util {

/*
 * A class to help write out a brotli byte stream which is a concatenation
 * of multi-bit values. Follows the bit packing conventions from:
 * https://datatracker.ietf.org/doc/html/rfc7932#section-1.5.1
 */
class BrotliBitBuffer {
 public:
  BrotliBitBuffer() : buffer_(), bit_index_(8) {
  }

  // Regular numbers are appened from LSB to MSB.
  void append_number(uint32_t bits, unsigned count);

  // Prefix codes are appened from MSB to LSB
  void append_prefix_code(uint32_t bits, unsigned count);

  absl::Span<const uint8_t> data() const;

 private:

  std::vector<uint8_t> buffer_;
  // index of the next bit to be written in the current byte,
  // value is in [0-7]
  unsigned bit_index_;
};

}  // namespace util

#endif  // UTIL_BROTLI_BIT_BUFFER_H_
