#include "patch_subset/cbor/patch_format_fields.h"

#include "patch_subset/cbor/array.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using patch_subset::PatchFormat;
using std::optional;
using std::vector;

StatusCode PatchFormatFields::ToPatchFormat(uint64_t value, PatchFormat* out) {
  if (out == nullptr) {
    return StatusCode::kInvalidArgument;
  }
  switch (value) {
    case PatchFormat::BROTLI_SHARED_DICT:
    case PatchFormat::VCDIFF: {
      *out = static_cast<PatchFormat>(value);
      return StatusCode::kOk;
    }
    default:
      return StatusCode::kInvalidArgument;
  }
}

StatusCode PatchFormatFields::Decode(const cbor_item_t& bytes,
                                     vector<PatchFormat>& out) {
  vector<uint64_t> int_values;
  StatusCode sc = Array::Decode(bytes, int_values);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  out.resize(int_values.size());
  out.clear();
  for (uint64_t int_value : int_values) {
    PatchFormat patch_format;
    sc = ToPatchFormat(int_value, &patch_format);
    if (sc == StatusCode::kOk) {
      out.push_back(patch_format);
    }
  }
  return StatusCode::kOk;
}

StatusCode PatchFormatFields::Encode(const vector<PatchFormat>& formats,
                                     cbor_item_unique_ptr& bytestring_out) {
  vector<uint64_t> int_values(formats.size());
  int_values.clear();
  for (PatchFormat format : formats) {
    int_values.push_back(format);
  }
  return Array::Encode(int_values, bytestring_out);
}

StatusCode PatchFormatFields::SetPatchFormatsListField(
    cbor_item_t& map, int field_number,
    const optional<vector<PatchFormat>>& format_list) {
  if (!format_list.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  StatusCode sc = Encode(format_list.value(), field_value);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

StatusCode PatchFormatFields::GetPatchFormatsListField(
    const cbor_item_t& map, int field_number,
    optional<vector<PatchFormat>>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  vector<PatchFormat> results;
  sc = Decode(*field, results);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(results);
  return StatusCode::kOk;
}

StatusCode PatchFormatFields::SetPatchFormatField(
    cbor_item_t& map, int field_number,
    const optional<PatchFormat>& format_list) {
  if (!format_list.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  return CborUtils::SetField(
      map, field_number, cbor_move(CborUtils::EncodeInt(format_list.value())));
}

StatusCode PatchFormatFields::GetPatchFormatField(const cbor_item_t& map,
                                                  int field_number,
                                                  optional<PatchFormat>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return sc;
  }
  int result_int;
  sc = CborUtils::DecodeInt(*field, &result_int);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  PatchFormat result;
  sc = ToPatchFormat(result_int, &result);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.emplace(result);
  return StatusCode::kOk;
}

}  // namespace patch_subset::cbor
