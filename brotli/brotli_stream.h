#ifndef BROTLI_BROTLI_STREAM_H_
#define BROTLI_BROTLI_STREAM_H_

#include <algorithm>

#include "absl/types/span.h"
#include "brotli/brotli_bit_buffer.h"
#include "brotli/shared_brotli_encoder.h"
#include "common/status.h"

namespace brotli {

/*
 * A helper class used to generate a brotli compressed stream.
 */
class BrotliStream {
 public:
  BrotliStream(unsigned window_bits, unsigned dictionary_size = 0,
               unsigned starting_offset = 0)
      : starting_offset_(starting_offset),
        uncompressed_size_(starting_offset),
        window_bits_(std::max(std::min(window_bits, 24u), 10u)),
        window_size_((1 << window_bits_) - 16),
        dictionary_size_(dictionary_size),
        buffer_() {}

  static unsigned WindowBitsFor(unsigned base_size, unsigned derived_size) {
    for (unsigned bits = 10; bits <= 24; bits++) {
      unsigned size = (1 << bits) - 16;
      if (base_size + derived_size < size) {
        return bits;
      }
    }
    return 24;
  }

  // Insert bytes into the uncompressed stream from the shared dictionary
  // from [offset, offset + length)
  [[nodiscard]] bool insert_from_dictionary(unsigned offset, unsigned length);

  // Insert bytes into the stream raw with no compression applied.
  void insert_uncompressed(absl::Span<const uint8_t> bytes);

  // Insert bytes and compress them. No shared dictionary is used.
  patch_subset::StatusCode insert_compressed(absl::Span<const uint8_t> bytes);

  // Insert bytes and compress them uses a portion of the full dictionary.
  // Where partial_dict is the dictionary bytes from [0, partial_dict.size())
  patch_subset::StatusCode insert_compressed_with_partial_dict(
      absl::Span<const uint8_t> bytes, absl::Span<const uint8_t> partial_dict);

  // Appends another stream onto this one. The other stream must have been
  // started with a starting_offset == this.uncompressed_size_.
  void append(BrotliStream& other) {
    byte_align();
    other.byte_align();
    buffer_.sink().insert(buffer_.sink().end(), other.buffer_.sink().begin(),
                          other.buffer_.sink().end());
    uncompressed_size_ += (other.uncompressed_size_ - other.starting_offset_);
  }

  // TODO(garretrieger): insert_compressed_with_partial_dict that is offset
  //                     from the start. Will need to disable static dict
  //                     references somehow. Can we set a empty custom static
  //                     dict?

  // Align the stream to the nearest byte boundary.
  void byte_align();

  // Align the end of uncompressed data with a 4 byte boundary. Padding with
  // zeroes as nescessary.
  void four_byte_align_uncompressed() {
    uint8_t zeroes[] = {0, 0, 0, 0};
    if (uncompressed_size_ % 4 != 0) {
      unsigned to_add = 4 - (uncompressed_size_ % 4);
      insert_uncompressed(absl::Span<const uint8_t>(zeroes, to_add));
    }
  }

  // Insert a meta-block that signals the end of the stream.
  void end_stream();

  absl::Span<const uint8_t> compressed_data() const { return buffer_.data(); }

  unsigned window_bits() const { return window_bits_; }
  unsigned dictionary_size() const { return dictionary_size_; }
  unsigned uncompressed_size() const { return uncompressed_size_; }

 private:
  EncoderStatePointer create_encoder(
      unsigned stream_offset,
      const BrotliEncoderPreparedDictionary* dictionary) const;

  bool add_mlen(unsigned size);

  void add_stream_header();

  void add_prefix_tree(unsigned code, unsigned width);

  unsigned starting_offset_;
  unsigned uncompressed_size_;
  unsigned window_bits_;
  unsigned window_size_;
  unsigned dictionary_size_;
  BrotliBitBuffer buffer_;
};

}  // namespace brotli

#endif  // BROTLI_BROTLI_STREAM_H_
