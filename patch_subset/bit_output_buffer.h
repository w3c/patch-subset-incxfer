#ifndef PATCH_SUBSET_BIT_OUTPUT_BUFFER_H_
#define PATCH_SUBSET_BIT_OUTPUT_BUFFER_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "patch_subset/branch_factor.h"

namespace patch_subset {

/*
 * A class for concatenating bits. 1..32 bits can be appended at a time.
 * The final result is returned as a std::string.
 */
class BitOutputBuffer {
 public:
  // Construct an initially empty BitOutputBuffer.
  BitOutputBuffer(BranchFactor branch_factor);

  // The lowest/rightmost bits of the value bits are appended. The number of
  // bits appended depends on the BranchFactor this BitOutputBuffer was
  // constructed with. For example with a BranchFactor of BF4, append(0b11100)
  // would append 1100, in the order 0, 0, 1, 1.
  void append(uint32_t bits);

  // Returns the bits as a string. The first bits written are in the first byte.
  // If there are not enough bits to fill out the last byte, 0s are added.
  std::string to_string();

 private:
  BranchFactor branch_factor;
  std::vector<unsigned char> buffer;
  bool first_nibble;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BIT_OUTPUT_BUFFER_H_
