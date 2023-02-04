#include "patch_subset/cbor/patch_request.h"

#include <memory>

#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/patch_format_fields.h"

namespace patch_subset::cbor {

using absl::Status;
using patch_subset::PatchFormat;
using std::string;
using std::vector;

PatchRequest::PatchRequest()
    : _protocol_version(std::nullopt),
      _accept_formats(std::nullopt),
      _codepoints_have(std::nullopt),
      _codepoints_needed(std::nullopt),
      _indices_have(std::nullopt),
      _indices_needed(std::nullopt),
      _ordering_checksum(std::nullopt),
      _original_font_checksum(std::nullopt),
      _base_checksum(std::nullopt),
      _connection_speed(std::nullopt) {}

PatchRequest::PatchRequest(PatchRequest&& other) noexcept
    : _protocol_version(other._protocol_version),
      _accept_formats(std::move(other._accept_formats)),
      _codepoints_have(std::move(other._codepoints_have)),
      _codepoints_needed(std::move(other._codepoints_needed)),
      _indices_have(std::move(other._indices_have)),
      _indices_needed(std::move(other._indices_needed)),
      _ordering_checksum(other._ordering_checksum),
      _original_font_checksum(other._original_font_checksum),
      _base_checksum(other._base_checksum),
      _connection_speed(other._connection_speed) {}

PatchRequest::PatchRequest(
    ProtocolVersion protocol_version, vector<PatchFormat> accept_formats,
    CompressedSet codepoints_have, CompressedSet codepoints_needed,
    CompressedSet indices_have, CompressedSet indices_needed,
    uint64_t ordering_checksum, uint64_t original_font_checksum,
    uint64_t base_checksum, ConnectionSpeed connection_speed)
    : _protocol_version(protocol_version),
      _accept_formats(accept_formats),
      _codepoints_have(codepoints_have),
      _codepoints_needed(codepoints_needed),
      _indices_have(indices_have),
      _indices_needed(indices_needed),
      _ordering_checksum(ordering_checksum),
      _original_font_checksum(original_font_checksum),
      _base_checksum(base_checksum),
      _connection_speed(connection_speed) {}

Status PatchRequest::Decode(const cbor_item_t& cbor_map,
                                PatchRequest& out) {
  if (!cbor_isa_map(&cbor_map)) {
    return absl::InvalidArgumentError("not a map.");
  }
  PatchRequest result;
  Status sc = CborUtils::GetProtocolVersionField(
      cbor_map, kProtocolVersionFieldNumber, result._protocol_version);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = PatchFormatFields::GetPatchFormatsListField(
      cbor_map, kAcceptPatchFormatsFieldNumber, result._accept_formats);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CompressedSet::GetCompressedSetField(
      cbor_map, kCodepointsHaveFieldNumber, result._codepoints_have);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CompressedSet::GetCompressedSetField(
      cbor_map, kCodepointsNeededFieldNumber, result._codepoints_needed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CompressedSet::GetCompressedSetField(cbor_map, kIndicesHaveFieldNumber,
                                            result._indices_have);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CompressedSet::GetCompressedSetField(cbor_map, kIndicesNeededFieldNumber,
                                            result._indices_needed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kOrderingChecksumFieldNumber,
                                 result._ordering_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kOriginalFontChecksumFieldNumber,
                                 result._original_font_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CborUtils::GetUInt64Field(cbor_map, kBaseChecksumFieldNumber,
                                 result._base_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  sc = CborUtils::GetConnectionSpeedField(cbor_map, kConnectionSpeedFieldNumber,
                                          result._connection_speed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field lookup failed");
  }
  out = std::move(result);
  return absl::OkStatus();
}

Status PatchRequest::Encode(cbor_item_unique_ptr& map_out) const {
  int size = (_protocol_version.has_value() ? 1 : 0) +
             (_accept_formats.has_value() ? 1 : 0) +
             (_codepoints_have.has_value() ? 1 : 0) +
             (_codepoints_needed.has_value() ? 1 : 0) +
             (_indices_have.has_value() ? 1 : 0) +
             (_indices_needed.has_value() ? 1 : 0) +
             (_ordering_checksum.has_value() ? 1 : 0) +
             (_original_font_checksum.has_value() ? 1 : 0) +
             (_base_checksum.has_value() ? 1 : 0) +
             (_connection_speed.has_value() ? 1 : 0);
  cbor_item_unique_ptr map = make_cbor_map(size);
  Status sc = CborUtils::SetProtocolVersionField(
      *map, kProtocolVersionFieldNumber, _protocol_version);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = PatchFormatFields::SetPatchFormatsListField(
      *map, kAcceptPatchFormatsFieldNumber, _accept_formats);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CompressedSet::SetCompressedSetField(*map, kCodepointsHaveFieldNumber,
                                            _codepoints_have);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CompressedSet::SetCompressedSetField(*map, kCodepointsNeededFieldNumber,
                                            _codepoints_needed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CompressedSet::SetCompressedSetField(*map, kIndicesHaveFieldNumber,
                                            _indices_have);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CompressedSet::SetCompressedSetField(*map, kIndicesNeededFieldNumber,
                                            _indices_needed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CborUtils::SetUInt64Field(*map, kOrderingChecksumFieldNumber,
                                 _ordering_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CborUtils::SetUInt64Field(*map, kOriginalFontChecksumFieldNumber,
                                 _original_font_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc =
      CborUtils::SetUInt64Field(*map, kBaseChecksumFieldNumber, _base_checksum);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  sc = CborUtils::SetConnectionSpeedField(*map, kConnectionSpeedFieldNumber,
                                          _connection_speed);
  if (!sc.ok()) {
    return absl::InvalidArgumentError("field setting failed.");
  }
  map_out.swap(map);
  return absl::OkStatus();
}

Status PatchRequest::ParseFromString(const std::string& buffer,
                                         PatchRequest& out) {
  cbor_item_unique_ptr item = empty_cbor_ptr();
  Status sc = CborUtils::DeserializeFromBytes(buffer, item);
  if (!sc.ok()) {
    return sc;
  }
  sc = Decode(*item, out);
  if (!sc.ok()) {
    return sc;
  }
  return absl::OkStatus();
}

Status PatchRequest::SerializeToString(std::string& out) const {
  cbor_item_unique_ptr item = empty_cbor_ptr();
  Status sc = Encode(item);
  if (!sc.ok()) {
    return sc;
  }
  unsigned char* buffer;
  size_t buffer_size;
  size_t written = cbor_serialize_alloc(item.get(), &buffer, &buffer_size);
  if (written == 0) {
    return absl::InternalError("cbor_serialize_alloc failed.");
  }
  out.assign(std::string((char*)buffer, written));
  free(buffer);
  return absl::OkStatus();
}

bool PatchRequest::HasProtocolVersion() const {
  return _protocol_version.has_value();
}
ProtocolVersion PatchRequest::GetProtocolVersion() const {
  return _protocol_version.value_or(ProtocolVersion::ONE);
}
PatchRequest& PatchRequest::SetProtocolVersion(ProtocolVersion version) {
  _protocol_version.emplace(version);
  return *this;
}
PatchRequest& PatchRequest::ResetProtocolVersion() {
  _protocol_version.reset();
  return *this;
}

bool PatchRequest::HasAcceptFormats() const {
  return _accept_formats.has_value();
}
static const vector<PatchFormat> kEmptyVector;
const vector<PatchFormat>& PatchRequest::AcceptFormats() const {
  if (_accept_formats.has_value()) {
    return _accept_formats.value();
  } else {
    return kEmptyVector;
  }
}
PatchRequest& PatchRequest::SetAcceptFormats(
    const vector<PatchFormat>& formats) {
  _accept_formats.emplace(formats);
  return *this;
}
PatchRequest& PatchRequest::AddAcceptFormat(patch_subset::PatchFormat format) {
  if (_accept_formats.has_value()) {
    _accept_formats->push_back(format);
  } else {
    _accept_formats.emplace(vector<PatchFormat>{format});
  }
  return *this;
}
PatchRequest& PatchRequest::ResetAcceptFormats() {
  _accept_formats.reset();
  return *this;
}

bool PatchRequest::HasCodepointsHave() const {
  return _codepoints_have.has_value();
}
static CompressedSet kEmptyCompressedSet;
const CompressedSet& PatchRequest::CodepointsHave() const {
  if (_codepoints_have.has_value()) {
    return _codepoints_have.value();
  } else {
    return kEmptyCompressedSet;
  }
}
PatchRequest& PatchRequest::SetCodepointsHave(const CompressedSet& codepoints) {
  _codepoints_have.emplace(codepoints);
  return *this;
}
PatchRequest& PatchRequest::ResetCodepointsHave() {
  _codepoints_have.reset();
  return *this;
}

bool PatchRequest::HasCodepointsNeeded() const {
  return _codepoints_needed.has_value();
}
const CompressedSet& PatchRequest::CodepointsNeeded() const {
  if (_codepoints_needed.has_value()) {
    return _codepoints_needed.value();
  } else {
    return kEmptyCompressedSet;
  }
}
PatchRequest& PatchRequest::SetCodepointsNeeded(
    const CompressedSet& codepoints) {
  _codepoints_needed.emplace(codepoints);
  return *this;
}
PatchRequest& PatchRequest::ResetCodepointsNeeded() {
  _codepoints_needed.reset();
  return *this;
}

bool PatchRequest::HasOrderingChecksum() const {
  return _ordering_checksum.has_value();
}
uint64_t PatchRequest::OrderingChecksum() const {
  return _ordering_checksum.has_value() ? _ordering_checksum.value() : 0;
}
PatchRequest& PatchRequest::SetOrderingChecksum(uint64_t checksum) {
  _ordering_checksum.emplace(checksum);
  return *this;
}
PatchRequest& PatchRequest::ResetOrderingChecksum() {
  _ordering_checksum.reset();
  return *this;
}

bool PatchRequest::HasOriginalFontChecksum() const {
  return _original_font_checksum.has_value();
}
uint64_t PatchRequest::OriginalFontChecksum() const {
  return _original_font_checksum.has_value() ? _original_font_checksum.value()
                                             : 0;
}
PatchRequest& PatchRequest::SetOriginalFontChecksum(uint64_t checksum) {
  _original_font_checksum.emplace(checksum);
  return *this;
}
PatchRequest& PatchRequest::ResetOriginalFontChecksum() {
  _original_font_checksum.reset();
  return *this;
}

bool PatchRequest::HasBaseChecksum() const {
  return _base_checksum.has_value();
}
uint64_t PatchRequest::BaseChecksum() const {
  return _base_checksum.has_value() ? _base_checksum.value() : 0;
}
PatchRequest& PatchRequest::SetBaseChecksum(uint64_t checksum) {
  _base_checksum.emplace(checksum);
  return *this;
}
PatchRequest& PatchRequest::ResetBaseChecksum() {
  _base_checksum.reset();
  return *this;
}

bool PatchRequest::HasConnectionSpeed() const {
  return _connection_speed.has_value();
}
// TODO: Is AVERAGE a good default value?
ConnectionSpeed PatchRequest::GetConnectionSpeed() const {
  return _connection_speed.has_value() ? _connection_speed.value()
                                       : ConnectionSpeed::AVERAGE;
}
PatchRequest& PatchRequest::SetConnectionSpeed(
    ConnectionSpeed connection_speed) {
  _connection_speed.emplace(connection_speed);
  return *this;
}
PatchRequest& PatchRequest::ResetConnectionSpeed() {
  _connection_speed.reset();
  return *this;
}

bool PatchRequest::HasIndicesHave() const { return _indices_have.has_value(); }
const CompressedSet& PatchRequest::IndicesHave() const {
  if (_indices_have.has_value()) {
    return _indices_have.value();
  } else {
    return kEmptyCompressedSet;
  }
}
PatchRequest& PatchRequest::SetIndicesHave(const CompressedSet& indices) {
  _indices_have.emplace(indices);
  return *this;
}
PatchRequest& PatchRequest::ResetIndicesHave() {
  _indices_have.reset();
  return *this;
}

bool PatchRequest::HasIndicesNeeded() const {
  return _indices_needed.has_value();
}
const CompressedSet& PatchRequest::IndicesNeeded() const {
  if (_indices_needed.has_value()) {
    return _indices_needed.value();
  } else {
    return kEmptyCompressedSet;
  }
}
PatchRequest& PatchRequest::SetIndicesNeeded(const CompressedSet& indices) {
  _indices_needed.emplace(indices);
  return *this;
}
PatchRequest& PatchRequest::ResetIndicesNeeded() {
  _indices_needed.reset();
  return *this;
}

string PatchRequest::ToString() const {
  string s = "";
  if (GetProtocolVersion() != ProtocolVersion::ONE) {
    s += "v" + std::to_string(GetProtocolVersion());
  }
  if (!AcceptFormats().empty()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "accept=[";
    int i = 0;
    for (PatchFormat format : AcceptFormats()) {
      if (i > 0) {
        s += ",";
      }
      s += std::to_string(format);
      i++;
    }
    s += "]";
  }
  if (HasCodepointsHave()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "cp_have=" + CodepointsHave().ToString();
  }
  if (HasCodepointsNeeded()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "cp_need=" + CodepointsNeeded().ToString();
  }
  if (HasIndicesHave()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "i_have=" + IndicesHave().ToString();
  }
  if (HasIndicesNeeded()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "i_need=" + IndicesNeeded().ToString();
  }
  if (HasOriginalFontChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "orig_cs=" + std::to_string(OriginalFontChecksum());
  }
  if (HasOrderingChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "ord_cs=" + std::to_string(OrderingChecksum());
  }
  if (HasBaseChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "base_cs=" + std::to_string(BaseChecksum());
  }
  if (HasConnectionSpeed()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "speed=" + std::to_string(ConnectionSpeed());
  }
  return "{" + s + "}";
}

PatchRequest& PatchRequest::operator=(PatchRequest&& other) noexcept {
  _protocol_version = other._protocol_version;
  _accept_formats = std::move(other._accept_formats);
  _codepoints_have = std::move(other._codepoints_have);
  _codepoints_needed = std::move(other._codepoints_needed);
  _indices_have = std::move(other._indices_have);
  _indices_needed = std::move(other._indices_needed);
  _ordering_checksum = other._ordering_checksum;
  _original_font_checksum = other._original_font_checksum;
  _base_checksum = other._base_checksum;
  _connection_speed = other._connection_speed;
  return *this;
}

bool PatchRequest::operator==(const PatchRequest& other) const {
  return _protocol_version == other._protocol_version &&
         _accept_formats == other._accept_formats &&
         _codepoints_have == other._codepoints_have &&
         _codepoints_needed == other._codepoints_needed &&
         _indices_have == other._indices_have &&
         _indices_needed == other._indices_needed &&
         _ordering_checksum == other._ordering_checksum &&
         _original_font_checksum == other._original_font_checksum &&
         _base_checksum == other._base_checksum &&
         _connection_speed == other._connection_speed;
}
bool PatchRequest::operator!=(const PatchRequest& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
