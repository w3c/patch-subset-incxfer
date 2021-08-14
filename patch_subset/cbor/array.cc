#include "patch_subset/cbor/array.h"

#include <vector>

#include "cbor.h"
#include "cbor_utils.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

StatusCode Array::EncodeIntegerArray(const std::vector<uint64_t>& ints,
                                     cbor_item_unique_ptr& array_out) {
  cbor_item_unique_ptr out = make_cbor_array(ints.size());
  for (uint64_t i : ints) {
    if (!cbor_array_push(out.get(), cbor_move(CborUtils::EncodeUInt64(i)))) {
      return StatusCode::kInternal;
    }
  }

  array_out.swap(out);
  return StatusCode::kOk;
}

StatusCode Array::DecodeIntegerArray(const cbor_item_t& array,
                                     std::vector<uint64_t>& out) {
  std::vector<uint64_t> decoded;
  for (size_t i = 0; i < cbor_array_size(&array); i++) {
    cbor_item_unique_ptr cbor_int = wrap_cbor_item(cbor_array_get(&array, i));
    uint64_t value;
    StatusCode code;
    if ((code = CborUtils::DecodeUInt64(*cbor_int, &value)) !=
        StatusCode::kOk) {
      return code;
    }
    out.push_back(value);
  }
  out.swap(decoded);
  return StatusCode::kOk;
}

StatusCode Array::SetIntegerArrayField(
    cbor_item_t& map, int field_number,
    const std::optional<std::vector<uint64_t>>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }

  cbor_item_unique_ptr encoded = empty_cbor_ptr();
  StatusCode code = EncodeIntegerArray(*value, encoded);
  if (code != StatusCode::kOk) {
    return code;
  }
  return CborUtils::SetField(map, field_number, move_out(encoded));
}

StatusCode Array::GetIntegerArrayField(
    const cbor_item_t& map, int field_number,
    std::optional<std::vector<uint64_t>>& out) {
  // TODO: update this to array
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  std::vector<uint64_t> results;
  sc = DecodeIntegerArray(*field, results);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(results);
  return StatusCode::kOk;
}

}  // namespace patch_subset::cbor
