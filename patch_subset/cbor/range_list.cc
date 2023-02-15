#include "patch_subset/cbor/range_list.h"

#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::Status;
using std::optional;
using std::vector;

Status RangeList::Decode(const cbor_item_t& array, range_vector& out) {
  vector<int32_t> ints;
  Status sc = IntegerList::DecodeSorted(array, ints);
  if (!sc.ok()) {
    return sc;
  }
  size_t size = ints.size();
  if (size % 2 != 0) {
    return absl::InvalidArgumentError(
        "Invalid number of ints! Can't make pairs.");
  }
  out.resize(size);
  out.clear();
  for (size_t i = 0; i < size; i += 2) {
    out.push_back(range(ints[i], ints[i + 1]));
  }
  return absl::OkStatus();
}

Status RangeList::Encode(const range_vector& ranges,
                         cbor_item_unique_ptr& bytestring_out) {
  size_t size = ranges.size();
  vector<int32_t> ints(2 * size);
  for (size_t i = 0; i < size; i++) {
    size_t j = 2 * i;
    if (ranges[i].first > INT32_MAX || ranges[i].second > INT32_MAX) {
      return absl::InvalidArgumentError("value is out of bounds.");
    }
    ints[j] = (int32_t)ranges[i].first;
    ints[j + 1] = (int32_t)ranges[i].second;
  }
  // EncodeSorted() will enforce sorting.
  return IntegerList::EncodeSorted(ints, bytestring_out);
}

Status RangeList::SetRangeListField(cbor_item_t& map, int field_number,
                                    const optional<range_vector>& int_list) {
  if (!int_list.has_value()) {
    return absl::OkStatus();  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  Status sc = Encode(int_list.value(), field_value);
  if (!sc.ok()) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

Status RangeList::GetRangeListField(const cbor_item_t& map, int field_number,
                                    optional<range_vector>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(map, field_number, field);
  if (absl::IsNotFound(sc)) {
    out.reset();
    return absl::OkStatus();
  } else if (!sc.ok()) {
    return sc;
  }
  range_vector results;
  sc = Decode(*field, results);
  if (!sc.ok()) {
    return sc;
  }
  out.emplace(results);
  return absl::OkStatus();
}

}  // namespace patch_subset::cbor
