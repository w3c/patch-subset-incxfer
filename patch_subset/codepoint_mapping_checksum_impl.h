#ifndef PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_IMPL_H_
#define PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_IMPL_H_

#include "patch_subset/codepoint_mapping_checksum.h"
#include "patch_subset/hasher.h"
#include "patch_subset/patch_subset.pb.h"

namespace patch_subset {

// Interface to a codepoint mapper.
class CodepointMappingChecksumImpl : public CodepointMappingChecksum {
 public:
  // Does not take ownership of hasher. hasher must live longer than this
  // object.
  CodepointMappingChecksumImpl(Hasher* hasher) : hasher_(hasher) {}

  uint64_t Checksum(const CodepointRemappingProto& response) const override;

 private:
  const Hasher* hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_IMPL_H_
