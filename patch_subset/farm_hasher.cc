#include "patch_subset/farm_hasher.h"

#include "absl/strings/string_view.h"
#include "farmhash.h"

using ::absl::string_view;

namespace patch_subset {

uint64_t FarmHasher::Checksum(string_view data) const {
  return ::util::Fingerprint(::util::Fingerprint64(data));
}

}  // namespace patch_subset
