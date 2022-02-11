#include "patch_subset/bit_input_buffer.h"

#include <string>

#include "absl/strings/string_view.h"

using absl::string_view;

namespace patch_subset {

static const unsigned int kBitsPerByte = 8;
static const unsigned int kBitsPerNibble = 4;
static const unsigned char kFirstNibbleMask = 0x0F;

static BranchFactor DecodeBranchFactor(unsigned char first_byte);
static unsigned int DecodeDepth(unsigned char first_byte);

BitInputBuffer::BitInputBuffer(string_view bits) {
  this->bits = bits;
  this->branch_factor = DecodeBranchFactor(bits[0]);
  depth = DecodeDepth(bits[0]);
  current_byte = 1;
  first_nibble = true;
}

const BranchFactor BitInputBuffer::GetBranchFactor() { return branch_factor; }

const unsigned int BitInputBuffer::Depth() { return depth; }

bool BitInputBuffer::read(uint32_t *out) {
  if (!out) {
    return false;
  }
  uint32_t result;
  switch (branch_factor) {
    case BF4:
      if (current_byte >= bits.size()) {
        return false;
      }
      if (first_nibble) {
        *out = (unsigned char)bits[current_byte] & kFirstNibbleMask;
        first_nibble = false;
      } else {
        *out = (unsigned char)bits[current_byte++] >> kBitsPerNibble;
        first_nibble = true;
      }
      break;
    case BF8:
      if (current_byte >= bits.size()) {
        return false;
      }
      *out = (unsigned char)bits[current_byte++];
      break;
    case BF16:
      if (current_byte + 1 >= bits.size()) {
        return false;
      }
      result = (unsigned char)bits[current_byte + 1];
      result = (result << kBitsPerByte) | (unsigned char)bits[current_byte];
      *out = result;
      current_byte += 2;
      break;
    case BF32:
      if (current_byte + 3 >= bits.size()) {
        return false;
      }
      result = (unsigned char)bits[current_byte + 3];
      result = (result << kBitsPerByte) | (unsigned char)bits[current_byte + 2];
      result = (result << kBitsPerByte) | (unsigned char)bits[current_byte + 1];
      result = (result << kBitsPerByte) | (unsigned char)bits[current_byte];
      *out = result;
      current_byte += 4;
      break;
  }
  return true;
}

static BranchFactor DecodeBranchFactor(unsigned char first_byte) {
  switch (first_byte & 0b11) {
    case 0b00:
      return BF4;
    case 0b01:
      return BF8;
    case 0b10:
      return BF16;
    case 0b11:
      return BF32;
    default:
      return BF8;
  }
}

static unsigned int DecodeDepth(unsigned char first_byte) {
  // Only look at bits 2..5.
  // Bits 0 and 1 are branch factor. Bits 6 and 7 are reserved for future use.
  return ((first_byte & 0b00111100u) >> 2) + 1;
}

}  // namespace patch_subset
