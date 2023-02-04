#include "patch_subset/cbor/array.h"

#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;

Status Array::Encode(const std::vector<uint64_t>& ints,
                         cbor_item_unique_ptr& array_out) {
  cbor_item_unique_ptr out = make_cbor_array(ints.size());
  for (uint64_t i : ints) {
    if (!cbor_array_push(out.get(), cbor_move(CborUtils::EncodeUInt64(i)))) {
      return absl::InternalError("cbor encoding failure.");
    }
  }

  array_out.swap(out);
  return absl::OkStatus();
}

Status Array::Decode(const cbor_item_t& array, std::vector<uint64_t>& out) {
  if (!cbor_isa_array(&array) || !cbor_array_is_definite(&array))
    return absl::InvalidArgumentError("not an array");

  std::vector<uint64_t> decoded;
  for (size_t i = 0; i < cbor_array_size(&array); i++) {
    cbor_item_unique_ptr cbor_int = wrap_cbor_item(cbor_array_get(&array, i));
    uint64_t value;
    Status code;
    if (!(code = CborUtils::DecodeUInt64(*cbor_int, &value)).ok()) {
      return code;
    }
    decoded.push_back(value);
  }
  out.swap(decoded);
  return absl::OkStatus();
}

Status Array::SetArrayField(
    cbor_item_t& map, int field_number,
    const std::optional<std::vector<uint64_t>>& int_list) {
  if (!int_list.has_value()) {
    return absl::OkStatus(); // Nothing to do.
  }

  cbor_item_unique_ptr encoded = empty_cbor_ptr();
  Status code = Encode(*int_list, encoded);
  if (!code.ok()) {
    return code;
  }
  return CborUtils::SetField(map, field_number, move_out(encoded));
}

Status Array::GetArrayField(const cbor_item_t& map, int field_number,
                                std::optional<std::vector<uint64_t>>& out) {
  // TODO: update this to array
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(map, field_number, field);
  if (absl::IsNotFound(sc)) {
    out.reset();
    return absl::OkStatus();
  } else if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failure.");
  }
  std::vector<uint64_t> results;
  sc = Decode(*field, results);
  if (!sc.ok()) {
    return sc;
  }
  out.emplace(results);
  return absl::OkStatus();
}

}  // namespace patch_subset::cbor
