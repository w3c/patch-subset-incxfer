#include "patch_subset/cbor/compressed_range_list.h"

#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/compressed_int_list.h"

namespace patch_subset::cbor {

using std::optional;
using std::vector;

StatusCode CompressedRangeList::Decode(const cbor_item_t& array,
                                       range_vector& out) {
  vector<int32_t> ints;
  StatusCode sc = CompressedIntList::DecodeSorted(array, ints);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  size_t size = ints.size();
  if (size % 2 != 0) {
    // Invalid number of ints! Can't make pairs.
    return StatusCode::kInvalidArgument;
  }
  out.resize(size);
  out.clear();
  for (size_t i = 0; i < size; i += 2) {
    out.push_back(range(ints[i], ints[i + 1]));
  }
  return StatusCode::kOk;
}

StatusCode CompressedRangeList::Encode(const range_vector& ranges,
                                       cbor_item_unique_ptr& bytestring_out) {
  size_t size = ranges.size();
  vector<int32_t> ints(2 * size);
  for (size_t i = 0; i < size; i++) {
    int j = 2 * i;
    ints[j] = ranges[i].first;
    ints[j + 1] = ranges[i].second;
  }
  // EncodeSorted() will enforce sorting.
  return CompressedIntList::EncodeSorted(ints, bytestring_out);
}

StatusCode CompressedRangeList::SetRangeListField(
    cbor_item_t& map, int field_number,
    const optional<range_vector>& int_list) {
  if (!int_list.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  StatusCode sc = Encode(int_list.value(), field_value);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

StatusCode CompressedRangeList::GetRangeListField(const cbor_item_t& map,
                                                  int field_number,
                                                  optional<range_vector>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  range_vector results;
  sc = Decode(*field, results);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  out.emplace(results);
  return StatusCode::kOk;
}

}  // namespace patch_subset::cbor
