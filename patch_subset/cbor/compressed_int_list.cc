#include "patch_subset/cbor/compressed_int_list.h"

#include "absl/strings/string_view.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/int_utils.h"

namespace patch_subset::cbor {

using absl::string_view;
using std::optional;
using std::vector;

StatusCode CompressedIntList::IsEmpty(const cbor_item_t& bytestring,
                                      bool* out) {
  if (!cbor_isa_bytestring(&bytestring)) {
    return StatusCode::kInvalidArgument;
  }
  *out = cbor_bytestring_length(&bytestring) == 0;
  return StatusCode::kOk;
}

StatusCode CompressedIntList::Decode(const cbor_item_t& bytestring,
                                     vector<int32_t>& out) {
  return Decode(bytestring, false, out);
}

StatusCode CompressedIntList::SetIntListField(
    cbor_item_t& map, int field_number,
    const optional<vector<int32_t>>& int_list) {
  if (!int_list.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  cbor_item_unique_ptr encoded = empty_cbor_ptr();
  StatusCode sc = Encode(int_list.value(), encoded);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(encoded));
}

StatusCode CompressedIntList::GetIntListField(const cbor_item_t& map,
                                              int field_number,
                                              optional<vector<int32_t>>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  vector<int32_t> results;
  sc = Decode(*field, results);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(results);
  return StatusCode::kOk;
}

StatusCode CompressedIntList::DecodeSorted(const cbor_item_t& bytestring,
                                           vector<int32_t>& out) {
  return Decode(bytestring, true, out);
}

StatusCode CompressedIntList::Decode(const cbor_item_t& bytestring, bool sorted,
                                     vector<int32_t>& out) {
  if (!cbor_isa_bytestring(&bytestring)) {
    return StatusCode::kInvalidArgument;
  }
  out.clear();
  size_t size = cbor_bytestring_length(&bytestring);
  auto* next_byte = (uint8_t*)cbor_bytestring_handle(&bytestring);
  int32_t current = 0;
  // Keep reading till we consume all the bytes.
  while (size > 0) {
    uint32_t udelta;
    size_t num_bytes;
    string_view sv((char*)next_byte, size);
    // Read zig-zag encoded unsigned int.
    StatusCode sc = IntUtils::UintBase128Decode(sv, &udelta, &num_bytes);
    if (sc != StatusCode::kOk) {
      return StatusCode::kInvalidArgument;
    }
    next_byte += num_bytes;
    size -= num_bytes;
    int32_t delta;
    if (sorted) {
      if (udelta <= INT32_MAX) {
        delta = udelta;
      } else {
        return StatusCode::kInvalidArgument;
      }
    } else {
      delta = IntUtils::ZigZagDecode(udelta);
    }
    int64_t current64 = (int64_t)current + (int64_t)delta;
    if (current64 < INT32_MIN || current64 > INT32_MAX) {
      return StatusCode::kInvalidArgument;
    }
    current = current64;
    out.push_back(current);
  }
  return StatusCode::kOk;
}

StatusCode CompressedIntList::Encode(const vector<int32_t>& ints, bool sorted,
                                     cbor_item_unique_ptr& bytestring_out) {
  if (ints.empty()) {
    bytestring_out.reset(cbor_build_bytestring(nullptr, 0));
    return StatusCode::kOk;
  }
  const size_t buffer_size = 5 * ints.size();
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  const uint8_t* buffer_start = buffer.get();
  uint8_t* next_byte = buffer.get();
  int32_t current = 0;
  for (int32_t n : ints) {
    int32_t delta = n - current;
    uint32_t udelta;
    if (sorted) {
      if (delta >= 0) {
        udelta = delta;
      } else {
        return StatusCode::kInvalidArgument;
      }
    } else {
      udelta = IntUtils::ZigZagEncode(delta);
    }
    int bytes_left = buffer_size - (next_byte - buffer_start);
    if (bytes_left < 0) {
      return StatusCode::kInternal;  // Not expected to happen.
    }
    size_t size_in_out = bytes_left;
    StatusCode sc =
        IntUtils::UIntBase128Encode(udelta, next_byte, &size_in_out);
    if (sc != StatusCode::kOk) {
      return StatusCode::kInvalidArgument;
    }
    next_byte += size_in_out;
    current = n;
  }
  size_t bytes_used = next_byte - buffer_start;
  bytestring_out.reset(cbor_build_bytestring(buffer_start, bytes_used));
  return StatusCode::kOk;
}

StatusCode CompressedIntList::Encode(const vector<int32_t>& ints,
                                     cbor_item_unique_ptr& bytestring_out) {
  return Encode(ints, false, bytestring_out);
}

StatusCode CompressedIntList::EncodeSorted(
    const vector<int32_t>& positive_sorted_ints,
    cbor_item_unique_ptr& bytestring_out) {
  return Encode(positive_sorted_ints, true, bytestring_out);
}

}  // namespace patch_subset::cbor
