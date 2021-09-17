#ifndef PATCH_SUBSET_INTEGER_LIST_CHECKSUM_IMPL_H_
#define PATCH_SUBSET_INTEGER_LIST_CHECKSUM_IMPL_H_

#include <cstdint>

#include "patch_subset/hasher.h"
#include "patch_subset/integer_list_checksum.h"

namespace patch_subset {

// Interface to a codepoint remapping checksum generator.
class IntegerListChecksumImpl : public IntegerListChecksum {
 public:
  // Does not take ownership of hasher. hasher must live longer than this
  // object.
  IntegerListChecksumImpl(Hasher* hasher) : hasher_(hasher) {}

  uint64_t Checksum(const std::vector<int32_t>& ints) const override;

 private:
  const Hasher* hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_INTEGER_LIST_CHECKSUM_IMPL_H_
