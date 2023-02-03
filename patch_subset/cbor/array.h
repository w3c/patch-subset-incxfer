#ifndef PATCH_SUBSET_CBOR_ARRAY_H_
#define PATCH_SUBSET_CBOR_ARRAY_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

class Array {
 public:
  static absl::StatusCode Encode(const std::vector<uint64_t>& ints,
                                 cbor_item_unique_ptr& array_out);

  static absl::StatusCode Decode(const cbor_item_t& array,
                                 std::vector<uint64_t>& out);

  static absl::StatusCode SetArrayField(
      cbor_item_t& map, int field_number,
      const std::optional<std::vector<uint64_t>>& int_list);

  static absl::StatusCode GetArrayField(
      const cbor_item_t& map, int field_number,
      std::optional<std::vector<uint64_t>>& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_ARRAY_H_
