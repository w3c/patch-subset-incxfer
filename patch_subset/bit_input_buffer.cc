#include "patch_subset/bit_input_buffer.h"

#include <string>

#include "absl/strings/string_view.h"

using absl::string_view;

namespace patch_subset {

static const uint32_t kBitsPerByte = 8;
static const uint32_t kBitsPerTwoBytes = 16;
static const uint32_t kBitsPerThreeBytes = 24;
static const uint32_t kBitsPerNibble = 4;
static const uint8_t kFirstNibbleMask = 0x0F;
static const uint8_t kFirstTwoBitsMask = 0b11;

static BranchFactor DecodeBranchFactor(string_view bits);
static uint32_t DecodeDepth(string_view bits);

BitInputBuffer::BitInputBuffer(string_view bits)
    : branch_factor(DecodeBranchFactor(bits)), depth(DecodeDepth(bits)) {
  this->bits = bits;
  current_byte = 1;
  current_pair = 0;
  first_nibble = true;
}

const BranchFactor BitInputBuffer::GetBranchFactor() { return branch_factor; }

const uint32_t BitInputBuffer::Depth() { return depth; }

bool BitInputBuffer::read(uint32_t *out) {
  if (!out) {
    return false;
  }
  switch (branch_factor) {
    case BF2:
      if (current_byte >= bits.size()) {
        return false;
      }
      *out = (bits[current_byte] >> (2 * current_pair)) & kFirstTwoBitsMask;
      current_pair++;
      if (current_pair == 4) {
        current_byte++;
        current_pair = 0;
      }
      break;
    case BF4:
      if (current_byte >= bits.size()) {
        return false;
      }
      if (first_nibble) {
        *out = (uint8_t)bits[current_byte] & kFirstNibbleMask;
        first_nibble = false;
      } else {
        *out = (uint8_t)bits[current_byte++] >> kBitsPerNibble;
        first_nibble = true;
      }
      break;
    case BF8:
      if (current_byte >= bits.size()) {
        return false;
      }
      *out = (uint8_t)bits[current_byte++];
      break;
    case BF32:
      if (current_byte + 3 >= bits.size()) {
        return false;
      }
      *out = ((uint8_t)bits[current_byte + 3] << kBitsPerThreeBytes) |
             ((uint8_t)bits[current_byte + 2] << kBitsPerTwoBytes) |
             ((uint8_t)bits[current_byte + 1] << kBitsPerByte) |
             (uint8_t)bits[current_byte];
      current_byte += 4;
      break;
  }
  return true;
}

static BranchFactor DecodeBranchFactor(string_view bits) {
  if (!bits.size()) {
    return BF2;
  }

  unsigned char first_byte = bits[0];
  switch (first_byte & 0b11) {
    case 0b00:
      return BF2;
    case 0b01:
      return BF4;
    case 0b10:
      return BF8;
    default:
      return BF32;
  }
}

static uint32_t DecodeDepth(string_view bits) {
  if (!bits.size()) {
    return 1;
  }

  // Only look at bits 2..5.
  // Bits 0 and 1 are branch factor. Bits 7 is reserved for future use.
  unsigned char first_byte = bits[0];
  return ((first_byte & 0b01111100u) >> 2) + 1;
}

}  // namespace patch_subset
