#include "patch_subset/bit_output_buffer.h"

#include <string>

#include "absl/strings/string_view.h"

using ::absl::string_view;
using std::string;

namespace patch_subset {

static const unsigned int kBitsPerByte = 8;
static const unsigned int kBitsPerNibble = 4;
static const unsigned char kFirstNibbleMask = 0x0F;

BitOutputBuffer::BitOutputBuffer(BranchFactor branch_factor) {
  this->branch_factor = branch_factor;
  first_nibble = true;
}

void BitOutputBuffer::append(uint32_t bits) {
  switch (branch_factor) {
    case BF4:
      if (first_nibble) {
        buffer.push_back((unsigned char)bits & kFirstNibbleMask);
        first_nibble = false;
      } else {
        buffer[buffer.size() - 1] =
            ((buffer[buffer.size() - 1] & kFirstNibbleMask) |
             (((unsigned char)bits & kFirstNibbleMask) << kBitsPerNibble));
        first_nibble = true;
      }
      break;
    case BF8:
      buffer.push_back((unsigned char)bits);
      break;
    case BF16:
      buffer.push_back((unsigned char)bits);
      bits >>= kBitsPerByte;
      buffer.push_back((unsigned char)bits);
      break;
    case BF32:
      buffer.push_back((unsigned char)bits);
      bits >>= kBitsPerByte;
      buffer.push_back((unsigned char)bits);
      bits >>= kBitsPerByte;
      buffer.push_back((unsigned char)bits);
      bits >>= kBitsPerByte;
      buffer.push_back((unsigned char)bits);
      break;
  }
}

std::string BitOutputBuffer::to_string() {
  return string{buffer.begin(), buffer.end()};
}
}  // namespace patch_subset
