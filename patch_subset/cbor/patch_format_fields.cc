#include "patch_subset/cbor/patch_format_fields.h"

#include "patch_subset/cbor/array.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;
using patch_subset::PatchFormat;
using std::optional;
using std::vector;

Status PatchFormatFields::ToPatchFormat(uint64_t value, PatchFormat* out) {
  if (out == nullptr) {
    return absl::InvalidArgumentError("out is null");
  }
  switch (value) {
    case PatchFormat::BROTLI_SHARED_DICT:
    case PatchFormat::VCDIFF: {
      *out = static_cast<PatchFormat>(value);
      return absl::OkStatus();
    }
    default:
      return absl::InvalidArgumentError("unrecognized format value.");
  }
}

Status PatchFormatFields::Decode(const cbor_item_t& bytes,
                                     vector<PatchFormat>& out) {
  vector<uint64_t> int_values;
  Status sc = Array::Decode(bytes, int_values);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("array decoding failed.");
  }
  out.resize(int_values.size());
  out.clear();
  for (uint64_t int_value : int_values) {
    PatchFormat patch_format;
    sc = ToPatchFormat(int_value, &patch_format);
    if (sc.ok()) {
      out.push_back(patch_format);
    }
  }
  return absl::OkStatus();
}

Status PatchFormatFields::Encode(const vector<PatchFormat>& formats,
                                     cbor_item_unique_ptr& bytestring_out) {
  vector<uint64_t> int_values(formats.size());
  int_values.clear();
  for (PatchFormat format : formats) {
    int_values.push_back(format);
  }
  return Array::Encode(int_values, bytestring_out);
}

Status PatchFormatFields::SetPatchFormatsListField(
    cbor_item_t& map, int field_number,
    const optional<vector<PatchFormat>>& format_list) {
  if (!format_list.has_value()) {
    return absl::OkStatus();  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  Status sc = Encode(format_list.value(), field_value);
  if (!sc.ok()) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

Status PatchFormatFields::GetPatchFormatsListField(
    const cbor_item_t& map, int field_number,
    optional<vector<PatchFormat>>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(map, field_number, field);
  if (absl::IsNotFound(sc)) {
    out.reset();
    return absl::OkStatus();
  } else if (!sc.ok()) {
    return sc;
  }
  vector<PatchFormat> results;
  sc = Decode(*field, results);
  if (!sc.ok()) {
    return sc;
  }
  out.emplace(results);
  return absl::OkStatus();
}

Status PatchFormatFields::SetPatchFormatField(
    cbor_item_t& map, int field_number,
    const optional<PatchFormat>& format_list) {
  if (!format_list.has_value()) {
    return absl::OkStatus();  // Nothing to do.
  }
  return CborUtils::SetField(
      map, field_number, cbor_move(CborUtils::EncodeInt(format_list.value())));
}

Status PatchFormatFields::GetPatchFormatField(const cbor_item_t& map,
                                                  int field_number,
                                                  optional<PatchFormat>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(map, field_number, field);
  if (absl::IsNotFound(sc)) {
    out.reset();
    return absl::OkStatus();
  } else if (!sc.ok()) {
    return sc;
  }
  int result_int;
  sc = CborUtils::DecodeInt(*field, &result_int);
  if (!sc.ok()) {
    return sc;
  }
  PatchFormat result;
  sc = ToPatchFormat(result_int, &result);
  if (!sc.ok()) {
    return sc;
  }
  out.emplace(result);
  return absl::OkStatus();
}

}  // namespace patch_subset::cbor
