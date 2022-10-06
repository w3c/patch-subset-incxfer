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
  BrotliStream(unsigned window_bits, unsigned dictionary_size=0) :
      uncompressed_size_(0),
      window_bits_(std::max(std::min(window_bits, 24u), 10u)),
      window_size_((1 << window_bits_) - 16),
      dictionary_size_(dictionary_size),
      buffer_() {}

  // Insert bytes into the uncompressed stream from the shared dictionary
  // from [offset, offset + length)
  void insert_from_dictionary(unsigned offset, unsigned length);

  // Insert bytes into the stream raw with no compression applied.
  void insert_uncompressed(absl::Span<const uint8_t> bytes);

  // Insert bytes and compress them. No shared dictionary is used.
  patch_subset::StatusCode insert_compressed(absl::Span<const uint8_t> bytes);

  // Insert bytes and compress them uses a portion of the full dictionary.
  // Where partial_dict is the dictionary bytes from [0, partial_dict.size())
  patch_subset::StatusCode insert_compressed_with_partial_dict(
      absl::Span<const uint8_t> bytes,
      absl::Span<const uint8_t> partial_dict);

  // TODO(garretrieger): insert_compressed_with_partial_dict that is offset
  //                     from the start. Will need to disable static dict references
  //                     somehow. Can we set a empty custom static dict?

  // Align the stream to the nearest byte boundary.
  void byte_align();

  // TODO(garretrieger): add methods:
  // - insert_compressed: insert bytes and apply brotli compression, no dictionary is used.
  // - insert_with_partial_dictionary: insert compressed bytes and use a subrange of the
  //                                   full dictionary to encode against.

  // Insert a meta-block that signals the end of the stream.
  void end_stream();

  absl::Span<const uint8_t> compressed_data() const {
    return buffer_.data();
  }

 private:

  EncoderStatePointer create_encoder(unsigned stream_offset,
                                     const BrotliEncoderPreparedDictionary* dictionary) const;

  bool add_mlen (unsigned size);

  void add_stream_header ();

  void add_prefix_tree (unsigned code, unsigned width);

  unsigned uncompressed_size_;
  unsigned window_bits_;
  unsigned window_size_;
  unsigned dictionary_size_;
  BrotliBitBuffer buffer_;
};

}  // namespace brotli

#endif  // BROTLI_BROTLI_STREAM_H_
