#ifndef PATCH_SUBSET_HASHER_H_
#define PATCH_SUBSET_HASHER_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace patch_subset {

// Computes checksums of binary data.
class Hasher {
 public:
  virtual ~Hasher() = default;

  // Compute checksum of the provided data.
  virtual uint64_t Checksum(::absl::string_view data) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_HASHER_H_
