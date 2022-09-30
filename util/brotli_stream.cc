#include "util/brotli_stream.h"

#include <utility>

namespace util {

constexpr unsigned MAX_METABLOCK_SIZE = (1 << 24);

void BrotliStream::insert_from_dictionary(unsigned offset, unsigned length) {
  if (!add_mlen(length)) {
    // Too big for one meta-block Break into multiple meta-blocks.
    insert_from_dictionary(offset, MAX_METABLOCK_SIZE);
    insert_from_dictionary(offset + MAX_METABLOCK_SIZE, length - MAX_METABLOCK_SIZE);
    return;
  }

  // TODO
}

void BrotliStream::insert_uncompressed(absl::Span<const uint8_t> bytes) {
  uint32_t size = bytes.size();

  if (!add_mlen(size)) {
    // Too big for one meta-block Break into multiple meta-blocks.
    insert_uncompressed(bytes.subspan(0, MAX_METABLOCK_SIZE));
    insert_uncompressed(bytes.subspan(MAX_METABLOCK_SIZE));
    return;
  }

  // For meta-block header format see: https://datatracker.ietf.org/doc/html/rfc7932#section-9.2
  buffer_.append_number(0b1, 1);                    // ISUNCOMPRESSED
  buffer_.pad_to_end_of_byte ();

  buffer_.append_raw(bytes);
  uncompressed_size_ += size;
}

void BrotliStream::end_stream() {
  buffer_.append_number(0b1,  1); // ISLAST
  buffer_.append_number(0b1,  1); // ISLASTEMPTY
  buffer_.pad_to_end_of_byte ();
}

bool BrotliStream::add_mlen (unsigned size) {
  uint32_t num_nibbles = 0;
  uint32_t num_nibbles_code = 0;
  if (size == 0) {
    // Empty meta-block
    buffer_.append_number(0b11, 2); // MNIBBLES
    buffer_.append_number(0b0,  1); // Reserved
    buffer_.append_number(0b00, 2); // MSKIPBYTES
    buffer_.pad_to_end_of_byte ();
    return true;
  } else if (size <= (1 << 16)) {
    num_nibbles = 4;
    num_nibbles_code = 0b00;
  } else if (size <= (1 << 20)) {
    num_nibbles = 5;
    num_nibbles_code = 0b01;
  } else if (size <= MAX_METABLOCK_SIZE) {
    num_nibbles = 6;
    num_nibbles_code = 0b10;
  } else {
    // Too big for one meta-block signal need to break into multiple meta-blocks.
    return false;
  }

  if (!uncompressed_size_) {
    add_stream_header();
  }

  // For meta-block header format see: https://datatracker.ietf.org/doc/html/rfc7932#section-9.2
  buffer_.append_number(0b0, 1);                    // ISLAST
  buffer_.append_number(num_nibbles_code, 2);       // MNIBBLES
  buffer_.append_number(size - 1, num_nibbles * 4); // MLEN - 1

  return true;
}

void BrotliStream::add_stream_header() {

  static std::pair<uint8_t, uint8_t> window_codes[] = {
    std::pair<uint8_t, uint8_t>(0b0100001, 7), // 10
    std::pair<uint8_t, uint8_t>(0b0110001, 7), // 11
    std::pair<uint8_t, uint8_t>(0b1000001, 7), // 12
    std::pair<uint8_t, uint8_t>(0b1010001, 7), // 13
    std::pair<uint8_t, uint8_t>(0b1100001, 7), // 14
    std::pair<uint8_t, uint8_t>(0b1110001, 7), // 15
    std::pair<uint8_t, uint8_t>(0b0, 1),       // 16
    std::pair<uint8_t, uint8_t>(0b0000001, 7), // 17
    std::pair<uint8_t, uint8_t>(0b0011, 4),    // 18
    std::pair<uint8_t, uint8_t>(0b0101, 4),    // 19
    std::pair<uint8_t, uint8_t>(0b0111, 4),    // 20
    std::pair<uint8_t, uint8_t>(0b1001, 4),    // 21
    std::pair<uint8_t, uint8_t>(0b1011, 4),    // 22
    std::pair<uint8_t, uint8_t>(0b1101, 4),    // 23
    std::pair<uint8_t, uint8_t>(0b1111, 4),    // 24
  };

  std::pair<uint8_t, uint8_t> code = window_codes[window_bits_ - 10];
  buffer_.append_number(code.first, code.second);
}

}  // namespace util