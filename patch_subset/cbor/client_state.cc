#include "patch_subset/cbor/client_state.h"

#include "absl/status/status.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::Status;
using std::string;
using std::vector;

ClientState::ClientState()
    : _original_font_checksum(std::nullopt),
      _codepoint_ordering(std::nullopt),
      _subset_axis_space(std::nullopt),
      _original_axis_space(std::nullopt) {}

ClientState::ClientState(uint64_t original_font_checksum,
                         const std::vector<int32_t>& codepoint_ordering,
                         const AxisSpace& subset_axis_space,
                         const AxisSpace& original_axis_space)
    : _original_font_checksum(original_font_checksum),
      _codepoint_ordering(codepoint_ordering),
      _subset_axis_space(subset_axis_space),
      _original_axis_space(original_axis_space) {}

ClientState::ClientState(ClientState&& other) noexcept
    : _original_font_checksum(other._original_font_checksum),
      _codepoint_ordering(std::move(other._codepoint_ordering)),
      _subset_axis_space(std::move(other._subset_axis_space)),
      _original_axis_space(std::move(other._original_axis_space)) {}

Status ClientState::Decode(const cbor_item_t& cbor_map, ClientState& out) {
  ClientState result;
  if (!cbor_isa_map(&cbor_map) || cbor_map_is_indefinite(&cbor_map)) {
    return absl::InvalidArgumentError("not a map.");
  }

  Status sc =
      CborUtils::GetUInt64Field(cbor_map, kOriginalFontChecksumFieldNumber,
                                result._original_font_checksum);

  sc.Update(IntegerList::GetIntegerListField(
      cbor_map, kCodepointOrderingFieldNumber, result._codepoint_ordering));

  sc.Update(AxisSpace::GetAxisSpaceField(cbor_map, kSubsetAxisSpaceFieldNumber,
                                         result._subset_axis_space));

  sc.Update(AxisSpace::GetAxisSpaceField(
      cbor_map, kOriginalAxisSpaceFieldNumber, result._original_axis_space));

  if (!sc.ok()) {
    return sc;
  }

  out = std::move(result);
  return absl::OkStatus();
}

Status ClientState::Encode(cbor_item_unique_ptr& out) const {
  int map_size = (_original_font_checksum.has_value() ? 1 : 0) +
                 (_codepoint_ordering.has_value() ? 1 : 0) +
                 (_subset_axis_space.has_value() ? 1 : 0) +
                 (_original_axis_space.has_value() ? 1 : 0);

  cbor_item_unique_ptr map = make_cbor_map(map_size);

  Status sc = CborUtils::SetUInt64Field(*map, kOriginalFontChecksumFieldNumber,
                                        _original_font_checksum);
  sc.Update(IntegerList::SetIntegerListField(
      *map, kCodepointOrderingFieldNumber, _codepoint_ordering));
  sc.Update(AxisSpace::SetAxisSpaceField(*map, kSubsetAxisSpaceFieldNumber,
                                         _subset_axis_space));
  sc.Update(AxisSpace::SetAxisSpaceField(*map, kOriginalAxisSpaceFieldNumber,
                                         _original_axis_space));

  if (!sc.ok()) {
    return sc;
  }

  out.swap(map);
  return absl::OkStatus();
}

Status ClientState::ParseFromString(const std::string& buffer,
                                    ClientState& out) {
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

Status ClientState::SerializeToString(std::string& out) const {
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

ClientState& ClientState::SetOriginalFontChecksum(uint64_t checksum) {
  _original_font_checksum.emplace(checksum);
  return *this;
}
ClientState& ClientState::ResetOriginalFontChecksum() {
  _original_font_checksum.reset();
  return *this;
}
bool ClientState::HasOriginalFontChecksum() const {
  return _original_font_checksum.has_value();
}
uint64_t ClientState::OriginalFontChecksum() const {
  return _original_font_checksum.has_value() ? _original_font_checksum.value()
                                             : 0;
}

ClientState& ClientState::SetCodepointOrdering(
    const vector<int32_t>& codepoint_ordering) {
  _codepoint_ordering.emplace(codepoint_ordering);
  return *this;
}
ClientState& ClientState::ResetCodepointOrdering() {
  _codepoint_ordering.reset();
  return *this;
}
bool ClientState::HasCodepointOrdering() const {
  return _codepoint_ordering.has_value();
}
static const vector<int32_t> kEmptyOrderings;
const vector<int32_t>& ClientState::CodepointOrdering() const {
  if (_codepoint_ordering.has_value()) {
    return _codepoint_ordering.value();
  } else {
    return kEmptyOrderings;
  }
}

ClientState& ClientState::SetSubsetAxisSpace(
    const AxisSpace& subset_axis_space) {
  _subset_axis_space.emplace(subset_axis_space);
  return *this;
}
ClientState& ClientState::ResetSubsetAxisSpace() {
  _subset_axis_space.reset();
  return *this;
}
bool ClientState::HasSubsetAxisSpace() const {
  return _subset_axis_space.has_value();
}
static const AxisSpace kEmptyAxisSpace;
const AxisSpace& ClientState::SubsetAxisSpace() const {
  if (_subset_axis_space.has_value()) {
    return _subset_axis_space.value();
  } else {
    return kEmptyAxisSpace;
  }
}

ClientState& ClientState::SetOriginalAxisSpace(
    const AxisSpace& original_axis_space) {
  _original_axis_space.emplace(original_axis_space);
  return *this;
}
ClientState& ClientState::ResetOriginalAxisSpace() {
  _original_axis_space.reset();
  return *this;
}
bool ClientState::HasOriginalAxisSpace() const {
  return _original_axis_space.has_value();
}
const AxisSpace& ClientState::OriginalAxisSpace() const {
  if (_original_axis_space.has_value()) {
    return _original_axis_space.value();
  } else {
    return kEmptyAxisSpace;
  }
}

string ClientState::ToString() const {
  string s;
  if (HasOriginalFontChecksum()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "orig_cs=" + std::to_string(OriginalFontChecksum());
  }
  if (HasCodepointOrdering()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "cp_rm=[";
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

  if (HasSubsetAxisSpace()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "subset_axis_space=" + SubsetAxisSpace().ToString();
  }

  if (HasOriginalAxisSpace()) {
    if (!s.empty()) {
      s += ",";
    }
    s += "original_axis_space=" + OriginalAxisSpace().ToString();
  }

  return "{" + s + "}";
}

ClientState& ClientState::operator=(ClientState&& other) noexcept {
  _original_font_checksum = other._original_font_checksum;
  _codepoint_ordering = std::move(other._codepoint_ordering);
  _subset_axis_space = std::move(other._subset_axis_space);
  _original_axis_space = std::move(other._original_axis_space);
  return *this;
}

bool ClientState::operator==(const ClientState& other) const {
  return _original_font_checksum == other._original_font_checksum &&
         _codepoint_ordering == other._codepoint_ordering &&
         _subset_axis_space == other._subset_axis_space &&
         _original_axis_space == other._original_axis_space;
}
bool ClientState::operator!=(const ClientState& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
