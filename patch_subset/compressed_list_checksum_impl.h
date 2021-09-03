#ifndef PATCH_SUBSET_COMPRESSED_LIST_CHECKSUM_IMPL_H_
#define PATCH_SUBSET_COMPRESSED_LIST_CHECKSUM_IMPL_H_

#include "patch_subset/compressed_list_checksum.h"
#include "patch_subset/hasher.h"
#include "patch_subset/patch_subset.pb.h"

namespace patch_subset {

// Interface to a codepoint mapper.
class CompressedListChecksumImpl : public CompressedListChecksum {
 public:
  // Does not take ownership of hasher. hasher must live longer than this
  // object.
  CompressedListChecksumImpl(Hasher* hasher) : hasher_(hasher) {}

  uint64_t Checksum(const CompressedListProto& response) const override;

 private:
  const Hasher* hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_COMPRESSED_LIST_CHECKSUM_IMPL_H_
