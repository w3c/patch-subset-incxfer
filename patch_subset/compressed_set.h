#ifndef PATCH_SUBSET_COMPRESSED_SET_H_
#define PATCH_SUBSET_COMPRESSED_SET_H_

#include <limits.h>

#include "common/status.h"
#include "hb.h"
#include "patch_subset/patch_subset.pb.h"

static_assert(CHAR_BIT == 8, "char's must have 8 bits for this library.");

namespace patch_subset {

class CompressedSet {
 public:
  static bool IsEmpty(const CompressedSetProto& set);

  // Decode a CompressedSet proto into an actual set.
  static StatusCode Decode(const CompressedSetProto& set, hb_set_t* out);

  // Encode a set of integers into a CompressedSet proto.
  static void Encode(const hb_set_t& set, CompressedSetProto* out);
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_COMPRESSED_SET_H_
