#include "patch_subset/cbor/patch_response.h"

#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"
#include "patch_subset/cbor/patch_format_fields.h"

namespace patch_subset::cbor {

using std::string;
using std::vector;

PatchResponse::PatchResponse()
    : _protocol_version(std::nullopt),
      _patch_format(std::nullopt),
      _patch(std::nullopt),
      _replacement(std::nullopt),
      _original_font_checksum(std::nullopt),
      _patched_checksum(std::nullopt),
      _codepoint_ordering(std::nullopt),
      _ordering_checksum(std::nullopt) {}

PatchResponse::PatchResponse(PatchResponse&& other) noexcept
    : _protocol_version(other._protocol_version),
      _patch_format(other._patch_format),
      _patch(std::move(other._patch)),
      _replacement(std::move(other._replacement)),
      _original_font_checksum(other._original_font_checksum),
      _patched_checksum(other._patched_checksum),
      _codepoint_ordering(std::move(other._codepoint_ordering)),
      _ordering_checksum(other._ordering_checksum) {}

PatchResponse::PatchResponse(ProtocolVersion protocol_version,
                             PatchFormat patch_format, string patch,
                             string replacement,
                             uint64_t original_font_checksum,
                             uint64_t patched_checksum,
                             vector<int32_t> codepoint_ordering,
                             uint64_t ordering_checksum)
    : _protocol_version(protocol_version),
      _patch_format(patch_format),
      _patch(patch),
      _replacement(replacement),
      _original_font_checksum(original_font_checksum),
      _patched_checksum(patched_checksum),
      _codepoint_ordering(codepoint_ordering),
      _ordering_checksum(ordering_checksum) {}

StatusCode PatchResponse::Decode(const cbor_item_t& cbor_map,
                                 PatchResponse& out) {
  if (!cbor_isa_map(&cbor_map)) {
    return StatusCode::kInvalidArgument;
  }
  PatchResponse result;

  StatusCode sc = CborUtils::GetProtocolVersionField(
      cbor_map, kProtocolVersionFieldNumber, result._protocol_version);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = PatchFormatFields::GetPatchFormatField(cbor_map, kPatchFormatFieldNumber,
                                              result._patch_format);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetBytesField(cbor_map, kPatchFieldNumber, result._patch);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetBytesField(cbor_map, kReplacementFieldNumber,
                                 result._replacement);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kOriginalFontChecksumFieldNumber,
                                 result._original_font_checksum);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kPatchedChecksumFieldNumber,
                                 result._patched_checksum);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = IntegerList::GetIntegerListField(cbor_map, kCodepointOrderingFieldNumber,
                                        result._codepoint_ordering);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kOrderingChecksumFieldNumber,
                                 result._ordering_checksum);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  out = std::move(result);
  return StatusCode::kOk;
}

StatusCode PatchResponse::Encode(cbor_item_unique_ptr& map_out) const {
  int size = (_protocol_version.has_value() ? 1 : 0) +
             (_patch_format.has_value() ? 1 : 0) +
             (_patch.has_value() ? 1 : 0) + (_replacement.has_value() ? 1 : 0) +
             (_original_font_checksum.has_value() ? 1 : 0) +
             (_patched_checksum.has_value() ? 1 : 0) +
             (_codepoint_ordering.has_value() ? 1 : 0) +
             (_ordering_checksum.has_value() ? 1 : 0);
  cbor_item_unique_ptr map = make_cbor_map(size);
  StatusCode sc = CborUtils::SetProtocolVersionField(
      *map, kProtocolVersionFieldNumber, _protocol_version);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = PatchFormatFields::SetPatchFormatField(*map, kPatchFormatFieldNumber,
                                              _patch_format);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetBytesField(*map, kPatchFieldNumber, _patch);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetBytesField(*map, kReplacementFieldNumber, _replacement);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetUInt64Field(*map, kOriginalFontChecksumFieldNumber,
                                 _original_font_checksum);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetUInt64Field(*map, kPatchedChecksumFieldNumber,
                                 _patched_checksum);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = IntegerList::SetIntegerListField(*map, kCodepointOrderingFieldNumber,
                                        _codepoint_ordering);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetUInt64Field(*map, kOrderingChecksumFieldNumber,
                                 _ordering_checksum);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  map_out.swap(map);
  return StatusCode::kOk;
}

StatusCode PatchResponse::ParseFromString(const std::string& buffer,
                                          PatchResponse& out) {
  cbor_item_unique_ptr item = empty_cbor_ptr();
  StatusCode sc = CborUtils::DeserializeFromBytes(buffer, item);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = Decode(*item, out);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  return StatusCode::kOk;
}

StatusCode PatchResponse::SerializeToString(std::string& out) const {
  cbor_item_unique_ptr item = empty_cbor_ptr();
  StatusCode sc = Encode(item);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  unsigned char* buffer;
  size_t buffer_size;
  size_t written = cbor_serialize_alloc(item.get(), &buffer, &buffer_size);
  if (written == 0) {
    return StatusCode::kInternal;
  }
  out.assign(std::string((char*)buffer, written));
  free(buffer);
  return StatusCode::kOk;
}

void PatchResponse::CopyTo(PatchResponse& target) const {
  if (HasProtocolVersion()) {
    target.SetProtocolVersion(ProtocolVersion());
  } else {
    target.ResetProtocolVersion();
  }
  if (HasPatchFormat()) {
    target.SetPatchFormat(GetPatchFormat());
  } else {
    target.ResetPatchFormat();
  }
  if (HasPatch()) {
    target.SetPatch(Patch());
  } else {
    target.ResetPatch();
  }
  if (HasReplacement()) {
    target.SetReplacement(Replacement());
  } else {
    target.ResetReplacement();
  }
  if (HasOriginalFontChecksum()) {
    target.SetOriginalFontChecksum(OriginalFontChecksum());
  } else {
    target.ResetOriginalFontChecksum();
  }
  if (HasPatchedChecksum()) {
    target.SetPatchedChecksum(PatchedChecksum());
  } else {
    target.ResetPatchedChecksum();
  }
  if (HasCodepointOrdering()) {
    target.SetCodepointOrdering(CodepointOrdering());
  } else {
    target.ResetCodepointOrdering();
  }
  if (HasOrderingChecksum()) {
    target.SetOrderingChecksum(OrderingChecksum());
  } else {
    target.ResetOrderingChecksum();
  }
}

bool PatchResponse::HasProtocolVersion() const {
  return _protocol_version.has_value();
}
ProtocolVersion PatchResponse::GetProtocolVersion() const {
  return _protocol_version.has_value() ? _protocol_version.value()
                                       : ProtocolVersion::ONE;
}
PatchResponse& PatchResponse::SetProtocolVersion(ProtocolVersion version) {
  _protocol_version.emplace(version);
  return *this;
}
PatchResponse& PatchResponse::ResetProtocolVersion() {
  _protocol_version.reset();
  return *this;
}

bool PatchResponse::HasPatchFormat() const { return _patch_format.has_value(); }
// TODO: Is this a good default value?
PatchFormat PatchResponse::GetPatchFormat() const {
  return _patch_format.has_value() ? _patch_format.value()
                                   : PatchFormat::BROTLI_SHARED_DICT;
}
PatchResponse& PatchResponse::SetPatchFormat(PatchFormat format) {
  _patch_format.emplace(format);
  return *this;
}
PatchResponse& PatchResponse::ResetPatchFormat() {
  _patch_format.reset();
  return *this;
}

bool PatchResponse::HasPatch() const { return _patch.has_value(); }
static const string kEmptyString;
const string& PatchResponse::Patch() const {
  if (_patch.has_value()) {
    return _patch.value();
  } else {
    return kEmptyString;
  }
}
PatchResponse& PatchResponse::SetPatch(const string& patch) {
  _patch.emplace(patch);
  return *this;
}
PatchResponse& PatchResponse::ResetPatch() {
  _patch.reset();
  return *this;
}

bool PatchResponse::HasReplacement() const { return _replacement.has_value(); }
const string& PatchResponse::Replacement() const {
  if (_replacement.has_value()) {
    return _replacement.value();
  } else {
    return kEmptyString;
  }
}
PatchResponse& PatchResponse::SetReplacement(const string& replacement) {
  _replacement.emplace(replacement);
  return *this;
}
PatchResponse& PatchResponse::ResetReplacement() {
  _replacement.reset();
  return *this;
}

bool PatchResponse::HasOriginalFontChecksum() const {
  return _original_font_checksum.has_value();
}
uint64_t PatchResponse::OriginalFontChecksum() const {
  return _original_font_checksum.has_value() ? _original_font_checksum.value()
                                             : 0;
}
PatchResponse& PatchResponse::SetOriginalFontChecksum(uint64_t checksum) {
  _original_font_checksum.emplace(checksum);
  return *this;
}
PatchResponse& PatchResponse::ResetOriginalFontChecksum() {
  _original_font_checksum.reset();
  return *this;
}

bool PatchResponse::HasPatchedChecksum() const {
  return _patched_checksum.has_value();
}
uint64_t PatchResponse::PatchedChecksum() const {
  return _patched_checksum.has_value() ? _patched_checksum.value() : 0;
}
PatchResponse& PatchResponse::SetPatchedChecksum(uint64_t checksum) {
  _patched_checksum.emplace(checksum);
  return *this;
}
PatchResponse& PatchResponse::ResetPatchedChecksum() {
  _patched_checksum.reset();
  return *this;
}

bool PatchResponse::HasCodepointOrdering() const {
  return _codepoint_ordering.has_value();
}
static const vector<int32_t> kEmptyVector;
const vector<int32_t>& PatchResponse::CodepointOrdering() const {
  if (_codepoint_ordering.has_value()) {
    return _codepoint_ordering.value();
  } else {
    return kEmptyVector;
  }
}
PatchResponse& PatchResponse::SetCodepointOrdering(
    const vector<int32_t>& codepoint_ordering) {
  _codepoint_ordering.emplace(codepoint_ordering);
  return *this;
}
PatchResponse& PatchResponse::ResetCodepointOrdering() {
  _codepoint_ordering.reset();
  return *this;
}

bool PatchResponse::HasOrderingChecksum() const {
  return _ordering_checksum.has_value();
}
uint64_t PatchResponse::OrderingChecksum() const {
  return _ordering_checksum.has_value() ? _ordering_checksum.value() : 0;
}
PatchResponse& PatchResponse::SetOrderingChecksum(uint64_t checksum) {
  _ordering_checksum.emplace(checksum);
  return *this;
}
PatchResponse& PatchResponse::ResetOrderingChecksum() {
  _ordering_checksum.reset();
  return *this;
}

string PatchResponse::ToString() const {
  string s = "";
  if (GetProtocolVersion() != ProtocolVersion::ONE) {
    s += "v" + std::to_string(GetProtocolVersion());
  }
  if (HasPatchFormat()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "fmt=" + std::to_string(GetPatchFormat());
  }
  if (HasPatch()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "patch=" + std::to_string(Patch().size()) + "b";
  }
  if (HasReplacement()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "patch=" + std::to_string(Replacement().size()) + "b";
  }
  if (HasPatchedChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "patched_cs=" + std::to_string(PatchedChecksum());
  }
  if (HasCodepointOrdering()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "ord=[";
    int i = 0;
    for (int32_t n : CodepointOrdering()) {
      if (i > 0) {
        s += ",";
      }
      s += std::to_string(n);
      i++;
    }
    s += "]";
  }
  if (HasOrderingChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "orig_cs=" + std::to_string(OriginalFontChecksum());
  }
  return "{" + s + "}";
}

PatchResponse& PatchResponse::operator=(PatchResponse&& other) noexcept {
  _protocol_version = other._protocol_version;
  _patch_format = other._patch_format;
  _patch = std::move(other._patch);
  _replacement = std::move(other._replacement);
  _original_font_checksum = other._original_font_checksum;
  _patched_checksum = other._patched_checksum;
  _codepoint_ordering = std::move(other._codepoint_ordering);
  _ordering_checksum = other._ordering_checksum;
  return *this;
}

bool PatchResponse::operator==(const PatchResponse& other) const {
  return _protocol_version == other._protocol_version &&
         _patch_format == other._patch_format && _patch == other._patch &&
         _replacement == other._replacement &&
         _original_font_checksum == other._original_font_checksum &&
         _patched_checksum == other._patched_checksum &&
         _codepoint_ordering == other._codepoint_ordering &&
         _ordering_checksum == other._ordering_checksum;
}
bool PatchResponse::operator!=(const PatchResponse& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
