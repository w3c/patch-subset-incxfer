#ifndef PATCH_SUBSET_BIT_INPUT_BUFFER_H_
#define PATCH_SUBSET_BIT_INPUT_BUFFER_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "patch_subset/branch_factor.h"

namespace patch_subset {

/*
 * A class for reading from an encoded sparse bit set. Groups of 4, 8, 16 or 32
 * bits will be read at a time, depending on the branch factor encoded in the
 * first byte of the data.
 */
class BitInputBuffer {
 public:
  BitInputBuffer(absl::string_view bits);

  const BranchFactor GetBranchFactor();
  const unsigned int Depth();

  // The lowest/rightmost bits of the value bits are set, the remaining are
  // cleared. The number of bits set depends on the BranchFactor this
  // BitInputBuffer was constructed with.
  bool read(uint32_t *out);

 private:
  BranchFactor branch_factor;
  uint32_t depth;
  absl::string_view bits;
  uint32_t current_byte;
  uint8_t current_pair;
  bool first_nibble;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BIT_INPUT_BUFFER_H_
