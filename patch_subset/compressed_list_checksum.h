#ifndef PATCH_COMPRESSED_LIST_MAPPING_CHECKSUM_H_
#define PATCH_COMPRESSED_LIST_MAPPING_CHECKSUM_H_

#include "patch_subset/patch_subset.pb.h"

namespace patch_subset {

// Interface to a codepoint mapper.
class CompressedListChecksum {
 public:
  virtual ~CompressedListChecksum() = default;

  // Compute a checksum for the provided CompressedListProto. This
  // checksum function must be stable. It should always return the same
  // value for the same input proto.
  virtual uint64_t Checksum(const CompressedListProto& proto) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_COMPRESSED_LIST_MAPPING_CHECKSUM_H_
