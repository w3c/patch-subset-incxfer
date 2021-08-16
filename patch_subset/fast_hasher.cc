#include "patch_subset/fast_hasher.h"

#include "absl/strings/string_view.h"
#include "fasthash.h"

using ::absl::string_view;

namespace patch_subset {

static const uint64_t seed = 0x11743e80f437ffe6;

uint64_t FastHasher::Checksum(string_view data) const {
  return fasthash64(data.data(), data.size(), seed);
}

}  // namespace patch_subset
