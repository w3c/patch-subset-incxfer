#ifndef PATCH_SUBSET_COMPRESSED_SET_H_
#define PATCH_SUBSET_COMPRESSED_SET_H_

#include <limits.h>

#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/cbor/compressed_set.h"

static_assert(CHAR_BIT == 8, "char's must have 8 bits for this library.");

namespace patch_subset {

class CompressedSet {
 public:
  // Decode a CBOR CompressedSet into a Harfbuzz set.
  static absl::StatusCode Decode(const patch_subset::cbor::CompressedSet& set,
                           hb_set_t* out);

  // Encode a Harfbuzz set of integers into a CBOR CompressedSet.
  static void Encode(const hb_set_t& set,
                     patch_subset::cbor::CompressedSet& out);
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_COMPRESSED_SET_H_
