#ifndef PATCH_SUBSET_CBOR_COMPRESSED_INTEGER_LIST_H_
#define PATCH_SUBSET_CBOR_COMPRESSED_INTEGER_LIST_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

/*
 * Compress lists of integers for encoding. The first value is stored, then
 * the remaining values are deltas between list elements. If values not sorted,
 * so the deltas could be negative, then "zig zag" encoding is applied to the
 * values. This reduces the compression somewhat - use the sorted methods if
 * applicable. Finally, the values are stored in a variable number of base-128
 * chunks, with the high bit indicating there are more chunks. This lets small
 * values be encoded in one byte.
 */
class IntegerList {
 public:
  static absl::Status IsEmpty(const cbor_item_t& bytestring, bool* out);

  // Create a compressed list given a list of integers.
  // Returns a cbor byte string.
  static absl::Status Encode(const std::vector<int32_t>& ints,
                             cbor_item_unique_ptr& bytestring_out);

  // Interpret a CBOR byte string as a compressed list of integers.
  static absl::Status Decode(const cbor_item_t& bytestring,
                             std::vector<int32_t>& out);

  static absl::Status SetIntegerListField(
      cbor_item_t& map, int field_number,
      const std::optional<std::vector<int32_t>>& int_list);
  static absl::Status GetIntegerListField(
      const cbor_item_t& map, int field_number,
      std::optional<std::vector<int32_t>>& out);

  // Create a compressed list given a sorted list of positive integers.
  // Giving up negative numbers, and negative deltas between integers, doubles
  // the range of integers that can be encoded in 1 byte.
  // Returns a cbor byte string.
  static absl::Status EncodeSorted(
      const std::vector<int32_t>& positive_sorted_ints,
      cbor_item_unique_ptr& bytestring_out);

  // Interpret a CBOR byte string as a compressed list of sorted positive
  // integers. Giving up negative numbers, and negative deltas between integers,
  // doubles the range of integers that can be encoded in 1 byte.
  static absl::Status DecodeSorted(const cbor_item_t& bytestring,
                                   std::vector<int32_t>& out);

 private:
  static absl::Status Decode(const cbor_item_t& bytestring, bool sorted,
                             std::vector<int32_t>& out);
  static absl::Status Encode(const std::vector<int32_t>& ints, bool sorted,
                             cbor_item_unique_ptr& bytestring_out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_COMPRESSED_INTEGER_LIST_H_
