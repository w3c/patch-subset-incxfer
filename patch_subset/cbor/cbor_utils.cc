#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::string_view;
using std::optional;
using std::set;
using std::string;

StatusCode CborUtils::GetField(const cbor_item_t& cbor_map, int field_number,
                               cbor_item_unique_ptr& out) {
  if (!cbor_isa_map(&cbor_map) || field_number < 0) {
    return StatusCode::kInvalidArgument;
  }
  size_t size = cbor_map_size(&cbor_map);
  cbor_pair* pairs = cbor_map_handle(&cbor_map);
  for (size_t i = 0; i < size; i++) {
    cbor_pair* pair = pairs + i;
    if (!cbor_is_int(pair->key)) {
      // Ill formed map. Keys should all be ints. Skip this field.
      continue;
    }
    int32_t id;
    StatusCode sc = DecodeInt(*pair->key, &id);
    if (sc != StatusCode::kOk) {
      // Ill formed map. Keys should all be valid ints. Skip this field.
      continue;
    }
    if (id == field_number) {
      // Because we are putting the pointer into a cbor_item_unique_ptr, which
      // will eventually call cbor_decref(), the item has a new "owner".
      if (!pair->value) {
        return StatusCode::kInvalidArgument;
      }
      out.reset(cbor_incref(pair->value));
      return StatusCode::kOk;
    }
  }
  // Field not found.
  out.reset(nullptr);
  return StatusCode::kNotFound;
}

// TODO: Templatize these methods.
StatusCode CborUtils::GetUInt64Field(const cbor_item_t& map, int field_number,
                                     optional<uint64_t>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  uint64_t result;
  sc = DecodeUInt64(*field, &result);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(result);
  return StatusCode::kOk;
}

StatusCode CborUtils::GetFloatField(const cbor_item_t& map, int field_number,
                                    optional<float>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  float result;
  sc = DecodeFloat(*field, &result);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(result);
  return StatusCode::kOk;
}

// TODO: This method could ignore failures, returning "", if we want.
StatusCode CborUtils::GetStringField(const cbor_item_t& map, int field_number,
                                     optional<string>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  string result;
  sc = DecodeString(*field, result);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(result);
  return StatusCode::kOk;
}

// TODO: This method could ignore failures, returning "", if we want.
StatusCode CborUtils::GetBytesField(const cbor_item_t& map, int field_number,
                                    optional<string>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  string result;
  sc = DecodeBytes(*field, result);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(result);
  return StatusCode::kOk;
}

StatusCode CborUtils::GetProtocolVersionField(const cbor_item_t& map,
                                              int field_number,
                                              optional<ProtocolVersion>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  int version_int;
  sc = DecodeInt(*field, &version_int);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  if (version_int == ProtocolVersion::ONE) {
    // Initially only version 1 is supported.
    out.emplace(ProtocolVersion::ONE);
  } else {
    out.reset();
  }
  return StatusCode::kOk;
}

StatusCode CborUtils::GetConnectionSpeedField(const cbor_item_t& map,
                                              int field_number,
                                              optional<ConnectionSpeed>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  int speed_int;
  sc = DecodeInt(*field, &speed_int);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  switch (speed_int) {
    case ConnectionSpeed::VERY_SLOW:
    case ConnectionSpeed::SLOW:
    case ConnectionSpeed::AVERAGE:
    case ConnectionSpeed::FAST:
    case ConnectionSpeed::VERY_FAST:
    case ConnectionSpeed::EXTREMELY_FAST: {
      out.emplace(static_cast<ConnectionSpeed>(speed_int));
      break;
    }
    default:
      out.reset();
  }
  return StatusCode::kOk;
}

StatusCode CborUtils::SetField(cbor_item_t& cbor_map, const int field_number,
                               cbor_item_t* field_value) {
  if (!cbor_isa_map(&cbor_map) || cbor_map_is_indefinite(&cbor_map) ||
      field_number < 0) {
    return StatusCode::kInvalidArgument;
  }
  cbor_item_t* key = EncodeInt(field_number);
  bool ok = cbor_map_add(
      &cbor_map, cbor_pair{.key = cbor_move(key), .value = field_value});
  if (!ok) {
    // The map failed to take ownership of the key. Undo the cbor_move().
    cbor_incref(key);
    // Now decrement the refcount to 0 and free the key.
    cbor_decref(&key);
    return StatusCode::kInternal;
  }
  return StatusCode::kOk;
}

StatusCode CborUtils::SetUInt64Field(cbor_item_t& map, int field_number,
                                     const optional<uint64_t>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeUInt64(value.value())));
}

StatusCode CborUtils::SetFloatField(cbor_item_t& map, int field_number,
                                    const optional<float>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeFloat(value.value())));
}

StatusCode CborUtils::SetStringField(cbor_item_t& map, int field_number,
                                     const optional<string>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeString(value.value())));
}

StatusCode CborUtils::SetBytesField(cbor_item_t& map, int field_number,
                                    const optional<string>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeBytes(value.value())));
}

StatusCode CborUtils::SetProtocolVersionField(
    cbor_item_t& map, int field_number,
    const optional<ProtocolVersion>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeInt(value.value())));
}

StatusCode CborUtils::SetConnectionSpeedField(
    cbor_item_t& map, int field_number,
    const optional<ConnectionSpeed>& value) {
  if (!value.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return SetField(map, field_number, cbor_move(EncodeInt(value.value())));
}

// TODO INT UTILS
static const int32_t MAX_INT8 = (1 << 8) - 1;
static const int32_t MIN_INT8 = -(1 << 8);
static const int32_t MAX_INT16 = (1 << 16) - 1;
static const int32_t MIN_INT16 = -(1 << 16);
static const int64_t MAX_INT32 = 4294967295L;  // (1 << 32) - 1;

// Use the least number of bits required.
// TODO: INT UTILS
cbor_item_t* CborUtils::EncodeInt(int32_t n) {
  if (n >= 0 && n <= MAX_INT8) {
    return cbor_build_uint8(n);
  } else if (n >= MIN_INT8 && n < 0) {
    // For a negative number N, we store |N + 1|, so there are not two zeros.
    // For example -1 --> 0, -2 --> 1, -3 --> 2 ...
    // The fact that the integer is negative is stored in a control bit.
    return cbor_build_negint8(abs(n) - 1);
  } else if (n > MAX_INT8 && n <= MAX_INT16) {
    return cbor_build_uint16(n);
  } else if (n >= MIN_INT16 && n < MIN_INT8) {
    return cbor_build_negint16(abs(n) - 1);
  } else if (n > MAX_INT16) {
    return cbor_build_uint32(n);
  } else {
    // delta < MIN_INT16
    return cbor_build_negint32(abs(n) - 1);
  }
}

StatusCode CborUtils::DecodeInt(const cbor_item_t& int_element, int32_t* out) {
  if (!cbor_is_int(&int_element) || out == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  uint64_t u64 = cbor_get_int(&int_element);
  if (cbor_isa_negint(&int_element)) {
    if (u64 >= UINT64_MAX) {
      return StatusCode::kInvalidArgument;
    }
    u64++;  // Undo CBOR encoding of negative integers.
    // Should never exceed 64 bit signed, but double check.
    if (u64 > INT64_MAX) {
      return StatusCode::kInvalidArgument;
    }
    auto n64 = (int64_t)u64;
    if (n64 < INT32_MIN || n64 > INT32_MAX) {
      return StatusCode::kInvalidArgument;
    }
    auto n32 = (int32_t)(-n64);  // Undo CBOR encoding of negative integers.
    *out = n32;
  } else {
    // Should never exceed 32 bit, but double check.
    if (u64 > INT64_MAX) {
      return StatusCode::kInvalidArgument;
    }
    auto n32 = (int32_t)u64;
    *out = n32;
  }
  return StatusCode::kOk;
}

// Use the least number of bits required.
cbor_item_t* CborUtils::EncodeUInt64(uint64_t n) {
  if (n <= MAX_INT8) {
    return cbor_build_uint8(n);
  } else if (n <= MAX_INT16) {
    return cbor_build_uint16(n);
  } else if (n <= MAX_INT32) {
    return cbor_build_uint32(n);
  } else {
    return cbor_build_uint64(n);
  }
}

StatusCode CborUtils::DecodeUInt64(const cbor_item_t& int_element,
                                   uint64_t* out) {
  if (!cbor_is_int(&int_element) || cbor_isa_negint(&int_element) ||
      out == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  *out = cbor_get_int(&int_element);
  return StatusCode::kOk;
}

cbor_item_t* CborUtils::EncodeFloat(float n) {
  // Specification states all floats are single precision.
  return cbor_build_float4(n);
}

StatusCode CborUtils::DecodeFloat(const cbor_item_t& float_element,
                                  float* out) {
  // Specification states all floats are single precision.
  if (!cbor_is_float(&float_element)
      || cbor_float_get_width(&float_element) != CBOR_FLOAT_32
      || out == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  *out = cbor_float_get_float4(&float_element);
  return StatusCode::kOk;
}

cbor_item_t* CborUtils::EncodeString(const string& s) {
  return cbor_build_stringn(s.c_str(), s.length());
}

StatusCode CborUtils::DecodeString(const cbor_item_t& string_item,
                                   string& out) {
  if (!cbor_isa_string(&string_item) ||
      !cbor_string_is_definite(&string_item)) {
    return StatusCode::kInvalidArgument;
  }
  size_t size = cbor_string_length(&string_item);
  unsigned char* handle = cbor_string_handle(&string_item);
  out = string((char*)handle, size);
  return StatusCode::kOk;
}

cbor_item_t* CborUtils::EncodeBytes(const string_view& bytes) {
  return cbor_build_bytestring((unsigned char*)bytes.data(), bytes.length());
}

StatusCode CborUtils::DecodeBytes(const cbor_item_t& bytes_item, string& out) {
  if (!cbor_isa_bytestring(&bytes_item) ||
      !cbor_bytestring_is_definite(&bytes_item)) {
    return StatusCode::kInvalidArgument;
  }
  size_t size = cbor_bytestring_length(&bytes_item);
  char* handle = (char*)cbor_bytestring_handle(&bytes_item);
  out = string(handle, size);
  return StatusCode::kOk;
}

set<uint64_t> CborUtils::MapKeys(const cbor_item_t& map) {
  set<uint64_t> keys;
  size_t size = cbor_map_size(&map);
  cbor_pair* pairs = cbor_map_handle(&map);
  for (size_t i = 0; i < size; i++) {
    cbor_pair* pair = pairs + i;
    keys.insert(cbor_get_int(pair->key));
  }
  return keys;
}

StatusCode CborUtils::SerializeToBytes(const cbor_item_t& item,
                                       string_view buffer,
                                       size_t* bytes_written) {
  if (bytes_written == nullptr || buffer.empty()) {
    return StatusCode::kInvalidArgument;
  }
  size_t bytes =
      cbor_serialize(&item, (unsigned char*)buffer.data(), buffer.size());
  if (bytes > 0) {
    *bytes_written = bytes;
    return StatusCode::kOk;
  } else {
    return StatusCode::kInvalidArgument;
  }
}

StatusCode CborUtils::DeserializeFromBytes(absl::string_view buffer,
                                           cbor_item_unique_ptr& out) {
  cbor_load_result result;
  cbor_item_t* item =
      cbor_load((unsigned char*)buffer.data(), buffer.size(), &result);
  if (item == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  cbor_item_unique_ptr wrapped = wrap_cbor_item(item);
  out.swap(wrapped);
  return StatusCode::kOk;
}

}  // namespace patch_subset::cbor
