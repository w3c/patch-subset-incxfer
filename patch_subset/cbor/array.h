#ifndef PATCH_SUBSET_CBOR_ARRAY_H_
#define PATCH_SUBSET_CBOR_ARRAY_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

class Array {
 public:
  static StatusCode Encode(const std::vector<uint64_t>& ints,
                           cbor_item_unique_ptr& array_out);

  static StatusCode Decode(const cbor_item_t& array,
                           std::vector<uint64_t>& out);

  static StatusCode SetArrayField(
      cbor_item_t& map, int field_number,
      const std::optional<std::vector<uint64_t>>& int_list);

  static StatusCode GetArrayField(
      const cbor_item_t& map, int field_number,
      std::optional<std::vector<uint64_t>>& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_ARRAY_H_
