#ifndef PATCH_SUBSET_INTEGER_LIST_MAPPING_CHECKSUM_H_
#define PATCH_SUBSET_INTEGER_LIST_MAPPING_CHECKSUM_H_

#include <vector>

namespace patch_subset {

// Interface to a codepoint remapping checksum generator.
class IntegerListChecksum {
 public:
  virtual ~IntegerListChecksum() = default;

  // Compute a checksum for the provided list of integers. This
  // checksum function must be stable. It should always return the same
  // value for the same input.
  virtual uint64_t Checksum(const std::vector<int32_t>& ints) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_INTEGER_LIST_MAPPING_CHECKSUM_H_
