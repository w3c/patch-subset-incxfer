#ifndef UTIL_BROTLI_STREAM_H_
#define UTIL_BROTLI_STREAM_H_

#include <algorithm>

#include "absl/types/span.h"
#include "util/brotli_bit_buffer.h"

namespace util {

/*
 * A helper class used to generate a brotli compressed stream.
 */
class BrotliStream {
 public:
  BrotliStream(unsigned window_bits) :
      uncompressed_size_(0),
      window_bits_(std::max(std::min(window_bits, 24u), 10u)),
      window_size_((1 << window_bits_) - 16),
      buffer_() {}

  // Insert bytes into the uncompressed stream from the shared dictionary
  // from [offset, offset + length)
  void insert_from_dictionary(unsigned offset, unsigned length);

  // Insert bytes into the stream raw with no compression applied.
  void insert_uncompressed(absl::Span<const uint8_t> bytes);

  // Insert a meta-block that signals the end of the stream.
  void end_stream();

  absl::Span<const uint8_t> compressed_data() const {
    return buffer_.data();
  }

 private:

  bool add_mlen (unsigned size);

  void add_stream_header ();

  void add_prefix_tree (unsigned code, unsigned width);

  unsigned uncompressed_size_;
  unsigned window_bits_;
  unsigned window_size_;
  BrotliBitBuffer buffer_;
};

}  // namespace util

#endif  // UTIL_BROTLI_STREAM_H_
