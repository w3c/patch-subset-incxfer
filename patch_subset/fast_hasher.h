#ifndef PATCH_SUBSET_FAST_HASHER_H_
#define PATCH_SUBSET_FAST_HASHER_H_

#include "absl/strings/string_view.h"
#include "patch_subset/hasher.h"

namespace patch_subset {

// Uses fast-hash (https://github.com/ztanml/fast-hash) to compute a checksum of
// binary data.
class FastHasher : public Hasher {
 public:
  FastHasher() {}
  ~FastHasher() override {}

  uint64_t Checksum(::absl::string_view data) const override;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_FAST_HASHER_H_
