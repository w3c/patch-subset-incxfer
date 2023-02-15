#include "patch_subset/cbor/int_utils.h"

namespace patch_subset::cbor {

using absl::Status;
using absl::string_view;

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
  if (unsigned_int & 1U) {
    if (unsigned_int == UINT32_MAX) {
      return INT32_MIN;
    }
    return (int32_t) - ((unsigned_int + 1U) / 2U);
  } else {
    return (int32_t)unsigned_int / 2U;
  }
}

Status IntUtils::UIntBase128Encode(uint32_t value, uint8_t* buffer,
                                   size_t* size_in_out) {
  if (buffer == nullptr || size_in_out == nullptr || *size_in_out <= 0) {
    return absl::InvalidArgumentError("buffer or size_in_out is null.");
  }

  size_t size = IntUtils::UIntBase128EncodedSize(value);
  if (*size_in_out < size) {
    return absl::InvalidArgumentError("not enough room in buffer.");
  }

  for (size_t i = 0; i < size; ++i) {
    int b = static_cast<int>((value >> (7 * (size - i - 1))) & 0x7f);
    if (i < size - 1) {
      b |= 0x80;
    }
    buffer[i] = b;
  }
  *size_in_out = size;
  return absl::OkStatus();
}

Status IntUtils::UIntBase128Decode(string_view bytes, uint32_t* uint_out,
                                   size_t* num_bytes_out) {
  if (bytes.data() == nullptr || bytes.empty() || uint_out == nullptr ||
      num_bytes_out == nullptr) {
    return absl::InvalidArgumentError("invalid arguments.");
  }

  uint32_t result = 0;
  unsigned i = 0;
  for (const auto c : absl::ClippedSubstr(bytes, 0, 5)) {
    unsigned char uc = (unsigned char)c;
    // No leading 0’s
    if (i == 0 && uc == 0x80) {
      return absl::InvalidArgumentError("invalid leading byte.");
    }

    // If any of the top seven bits are set then we're about to overflow.
    if (result & 0xFE000000) {
      return absl::InvalidArgumentError("overflow.");
    }

    result = (result << 7) | (uc & 0x7f);

    // Spin until most significant bit of data byte is false
    if ((uc & 0x80) == 0) {
      *uint_out = result;
      *num_bytes_out = i + 1;
      return absl::OkStatus();
    }

    i++;
  }

  return absl::InvalidArgumentError("missing final byte.");
}

int IntUtils::UIntBase128EncodedSize(uint32_t unsigned_int) {
  int size = 1;
  for (; unsigned_int >= 128; unsigned_int >>= 7) {
    ++size;
  }
  return size;
}

}  // namespace patch_subset::cbor
