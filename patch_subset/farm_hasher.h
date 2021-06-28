#ifndef PATCH_SUBSET_FARM_HASHER_H_
#define PATCH_SUBSET_FARM_HASHER_H_

#include "absl/strings/string_view.h"
#include "patch_subset/hasher.h"

namespace patch_subset {

// Uses farmhash to compute a checksum of binary data.
class FarmHasher : public Hasher {
 public:
  FarmHasher() {}
  ~FarmHasher() override {}

  uint64_t Checksum(::absl::string_view data) const override;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_FARM_HASHER_H_
