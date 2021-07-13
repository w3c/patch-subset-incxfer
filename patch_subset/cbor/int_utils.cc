#include "patch_subset/cbor/int_utils.h"

namespace patch_subset::cbor {

using std::string_view;

uint32_t IntUtils::ZigZagEncode(int32_t signed_int) {
  if (signed_int >= 0) {
    return ((uint32_t)signed_int) * 2;
  } else {
    // Need to be careful to avoid overflow while converting.
    uint64_t tmp = signed_int;
    tmp *= -2;
    return (uint32_t)(tmp - 1);
  }
}

int32_t IntUtils::ZigZagDecode(uint32_t unsigned_int) {
  if (unsigned_int & 1) {
    if (unsigned_int == UINT32_MAX) {
      return INT32_MIN;
    }
    return -((unsigned_int + 1) / 2);
  } else {
    return unsigned_int / 2;
  }
}

// Perhaps pass in vector<> and resize as needed?
StatusCode IntUtils::UIntBase128Encode(uint32_t unsigned_int, uint8_t* buffer,
                                       size_t* size_in_out) {
  if (buffer == nullptr || size_in_out == nullptr || *size_in_out <= 0) {
    return StatusCode::kInvalidArgument;
  }
  size_t num_bytes = 0;
  size_t size = *size_in_out;
  bool more_bytes;
  do {
    if (size == 0) {
      return StatusCode::kInvalidArgument;
    }
    uint8_t last_7_bits = unsigned_int & 0b01111111;
    unsigned_int >>= 7;
    more_bytes = unsigned_int != 0;
    if (more_bytes) {
      last_7_bits |= 0b10000000;  // Signal more bytes to come.
    }
    *buffer++ = last_7_bits;
    size--;
    num_bytes++;
  } while (more_bytes);
  *size_in_out = num_bytes;
  return StatusCode::kOk;
}

StatusCode IntUtils::UintBase128Decode(string_view bytes, uint32_t* uint_out,
                                       size_t* num_bytes_out) {
  if (bytes.empty() || uint_out == nullptr || num_bytes_out == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  const char* buffer = bytes.data();
  if (buffer == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  size_t size = bytes.size();
  size_t num_bytes = 0;
  uint32_t result = 0;
  bool high_bit;
  do {
    if (num_bytes >= 5) {
      // At most 5 bytes are used. Don't read any more.
      return StatusCode::kInvalidArgument;
    }
    if (size == 0) {
      // Ran out of bytes to read.
      return StatusCode::kInvalidArgument;
    }
    uint8_t next_7_bits = *buffer++;
    size--;
    num_bytes++;
    high_bit = next_7_bits & 0b10000000;
    next_7_bits &= 0b01111111;
    int shift = 7 * ((int)num_bytes - 1);
    if (shift == 5 && next_7_bits & 0b1110000) {
      // Shifting bits would overflow uint_32.
      return StatusCode::kInvalidArgument;
    }
    result = (next_7_bits << shift) | result;
  } while (high_bit);
  *uint_out = result;
  *num_bytes_out = num_bytes;
  return StatusCode::kOk;
}

int IntUtils::UintBase128EncodedSize(uint32_t uint) {
  int num_bytes = 0;
  do {
    uint >>= 7;
    num_bytes++;
  } while (uint);
  return num_bytes;
}

}  // namespace patch_subset::cbor
