#ifndef PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_H_
#define PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_H_

#include "patch_subset/patch_subset.pb.h"

namespace patch_subset {

// Interface to a codepoint mapper.
class CodepointMappingChecksum {
 public:
  virtual ~CodepointMappingChecksum() = default;

  // Compute a checksum for the provided CodepointRemappingProto. This
  // checksum function must be stable. It should always return the same
  // value for the same input proto.
  virtual uint64_t Checksum(const CodepointRemappingProto& response) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_CODEPOINT_MAPPING_CHECKSUM_H_
