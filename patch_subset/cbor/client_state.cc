#include "patch_subset/cbor/client_state.h"

#include "common/status.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/compressed_int_list.h"

namespace patch_subset::cbor {

using std::string;
using std::vector;

ClientState::ClientState()
    : _font_id(std::nullopt),
      _font_data(std::nullopt),
      _fingerprint(std::nullopt),
      _codepoint_remapping(std::nullopt) {}

ClientState::ClientState(const string& font_id, const string& font_data,
                         uint64_t fingerprint,
                         const vector<int32_t>& codepoint_remapping)
    : _font_id(font_id),
      _font_data(font_data),
      _fingerprint(fingerprint),
      _codepoint_remapping(codepoint_remapping) {}

StatusCode ClientState::Decode(const cbor_item_t& cbor_map, ClientState& out) {
  ClientState result;
  if (!cbor_isa_map(&cbor_map) || cbor_map_is_indefinite(&cbor_map)) {
    return StatusCode::kInvalidArgument;
  }
  StatusCode sc =
      CborUtils::GetStringField(cbor_map, kFontIdFieldNumber, result._font_id);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetBytesField(cbor_map, kFontDataFieldNumber,
                                result._font_data);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kFingerprintFieldNumber,
                                 result._fingerprint);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = CompressedIntList::GetIntListField(
      cbor_map, kCodepointRemappingFieldNumber, result._codepoint_remapping);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  /*
   * TODO
   * This will result in copying the fontdata field, which could be sizable.
   * Can you add a swap() method to ClientState which internally will use swap()
   * on the fontdata string. Or alternatively implement a move constructor for
   * ClientState and use std::move() here.
   */
  out = result;
  return StatusCode::kOk;
}

StatusCode ClientState::Encode(cbor_item_unique_ptr& out) const {
  int map_size = (_font_id.has_value() ? 1 : 0) +
                 (_font_data.has_value() ? 1 : 0) +
                 (_fingerprint.has_value() ? 1 : 0) +
                 (_codepoint_remapping.has_value() ? 1 : 0);
  cbor_item_unique_ptr map = make_cbor_map(map_size);
  StatusCode sc = CborUtils::SetStringField(*map, kFontIdFieldNumber, _font_id);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetBytesField(*map, kFontDataFieldNumber, _font_data);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CborUtils::SetUInt64Field(*map, kFingerprintFieldNumber, _fingerprint);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  sc = CompressedIntList::SetIntListField(*map, kCodepointRemappingFieldNumber,
                                          _codepoint_remapping);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  out.swap(map);
  return StatusCode::kOk;
}

ClientState& ClientState::SetFontId(const string& font_id) {
  _font_id.emplace(font_id);
  return *this;
}
ClientState& ClientState::ResetFontId() {
  _font_id.reset();
  return *this;
}
bool ClientState::HasFontId() const { return _font_id.has_value(); }
const string kEmptyString;
const string& ClientState::FontId() const {
  if (_font_id.has_value()) {
    return _font_id.value();
  } else {
    return kEmptyString;
  }
}

ClientState& ClientState::SetFontData(const string& font_data) {
  _font_data.emplace(font_data);
  return *this;
}
ClientState& ClientState::ResetFontData() {
  _font_data.reset();
  return *this;
}
bool ClientState::HasFontData() const { return _font_data.has_value(); }
const string& ClientState::FontData() const {
  if (_font_data.has_value()) {
    return _font_data.value();
  } else {
    return kEmptyString;
  }
}

ClientState& ClientState::SetFingerprint(uint64_t fingerprint) {
  _fingerprint.emplace(fingerprint);
  return *this;
}
ClientState& ClientState::ResetFingerprint() {
  _fingerprint.reset();
  return *this;
}
bool ClientState::HasFingerprint() const { return _fingerprint.has_value(); }
uint64_t ClientState::Fingerprint() const {
  return _fingerprint.has_value() ? _fingerprint.value() : 0;
}

ClientState& ClientState::SetCodepointRemapping(
    const vector<int32_t>& codepoint_remapping) {
  _codepoint_remapping.emplace(codepoint_remapping);
  return *this;
}
ClientState& ClientState::ResetCodepointRemapping() {
  _codepoint_remapping.reset();
  return *this;
}
bool ClientState::HasCodepointRemapping() const {
  return _codepoint_remapping.has_value();
}
static const vector<int32_t> kEmptyRemappings;
const vector<int32_t>& ClientState::CodepointRemapping() const {
  if (_codepoint_remapping.has_value()) {
    return _codepoint_remapping.value();
  } else {
    return kEmptyRemappings;
  }
}

bool ClientState::operator==(const ClientState& other) const {
  return _font_id == other._font_id && _font_data == other._font_data &&
         _fingerprint == other._fingerprint &&
         _codepoint_remapping == other._codepoint_remapping;
}
bool ClientState::operator!=(const ClientState& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
