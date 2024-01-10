#ifndef COMMON_FAST_HASHER_H_
#define COMMON_FAST_HASHER_H_

#include "absl/strings/string_view.h"
#include "common/hasher.h"

namespace common {

// Uses fast-hash (https://github.com/ztanml/fast-hash) to compute a checksum of
// binary data.
class FastHasher : public Hasher {
 public:
  FastHasher() {}
  ~FastHasher() override {}

  uint64_t Checksum(::absl::string_view data) const override;
};

}  // namespace common

#endif  // COMMON_FAST_HASHER_H_
