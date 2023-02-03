#include "patch_subset/cbor/compressed_set.h"

#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::StatusCode;
using absl::string_view;
using std::optional;
using std::string;

CompressedSet::CompressedSet()
    : _sparse_bit_set_bytes(std::nullopt), _ranges(std::nullopt) {}

CompressedSet::CompressedSet(CompressedSet&& other) noexcept
    : _sparse_bit_set_bytes(std::move(other._sparse_bit_set_bytes)),
      _ranges(std::move(other._ranges)) {}

CompressedSet::CompressedSet(string_view sparse_bit_set_bytes,
                             const range_vector& ranges)
    : _sparse_bit_set_bytes(sparse_bit_set_bytes), _ranges(ranges) {}

bool CompressedSet::empty() const {
  return (!_sparse_bit_set_bytes || _sparse_bit_set_bytes->empty()) &&
         (!_ranges || _ranges->empty());
}

StatusCode CompressedSet::Decode(const cbor_item_t& cbor_map,
                                 CompressedSet& out) {
  if (!cbor_isa_map(&cbor_map) || cbor_map_is_indefinite(&cbor_map)) {
    return StatusCode::kInvalidArgument;
  }
  CompressedSet result;
  StatusCode sc = CborUtils::GetBytesField(cbor_map, kSparseBitSetFieldNumber,
                                           result._sparse_bit_set_bytes);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = RangeList::GetRangeListField(cbor_map, kSRangeDeltasFieldNumber,
                                    result._ranges);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  out = std::move(result);
  return StatusCode::kOk;
}

StatusCode CompressedSet::Encode(cbor_item_unique_ptr& map_out) const {
  int size = (_sparse_bit_set_bytes.has_value() ? 1 : 0) +
             (_ranges.has_value() ? 1 : 0);
  cbor_item_unique_ptr map = make_cbor_map(size);
  StatusCode sc = CborUtils::SetBytesField(*map, kSparseBitSetFieldNumber,
                                           _sparse_bit_set_bytes);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  sc = RangeList::SetRangeListField(*map, kSRangeDeltasFieldNumber, _ranges);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  map_out.swap(map);
  return StatusCode::kOk;
}

StatusCode CompressedSet::SetCompressedSetField(
    cbor_item_t& map, int field_number,
    const optional<CompressedSet>& compressed_set) {
  if (!compressed_set.has_value()) {
    return StatusCode::kOk;  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  StatusCode sc = compressed_set.value().Encode(field_value);
  if (sc != StatusCode::kOk) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

StatusCode CompressedSet::GetCompressedSetField(const cbor_item_t& map,
                                                int field_number,
                                                optional<CompressedSet>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  StatusCode sc = CborUtils::GetField(map, field_number, field);
  if (sc == StatusCode::kNotFound) {
    out.reset();
    return StatusCode::kOk;
  } else if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  CompressedSet results;
  sc = Decode(*field, results);
  if (sc != StatusCode::kOk) {
    return StatusCode::kInvalidArgument;
  }
  out.emplace(results);
  return StatusCode::kOk;
}

bool CompressedSet::HasSparseBitSetBytes() const {
  return _sparse_bit_set_bytes.has_value();
}
CompressedSet& CompressedSet::SetSparseBitSetBytes(const string& bytes) {
  _sparse_bit_set_bytes.emplace(bytes);
  return *this;
}
CompressedSet& CompressedSet::ResetSparseBitSetBytes() {
  _sparse_bit_set_bytes.reset();
  return *this;
}
static const string kEmptyString;
const string& CompressedSet::SparseBitSetBytes() const {
  if (_sparse_bit_set_bytes.has_value()) {
    return _sparse_bit_set_bytes.value();
  } else {
    return kEmptyString;
  }
}

bool CompressedSet::HasRanges() const { return _ranges.has_value(); }
CompressedSet& CompressedSet::SetRanges(const range_vector& ranges) {
  _ranges.emplace(ranges);
  return *this;
}
CompressedSet& CompressedSet::AddRange(const range range) {
  if (_ranges.has_value()) {
    _ranges->push_back(range);
  } else {
    _ranges = range_vector{range};
  }
  return *this;
}
CompressedSet& CompressedSet::ResetRanges() {
  _ranges.reset();
  return *this;
}
static const range_vector kEmptyRanges;
const range_vector& CompressedSet::Ranges() const {
  if (_ranges.has_value()) {
    return _ranges.value();
  } else {
    return kEmptyRanges;
  }
}

string CompressedSet::ToString() const {
  string s = "{";
  int i = 0;
  for (range range : Ranges()) {
    if (i > 0) {
      s += ",";
    }
    s += "[" + std::to_string(range.first) + "-" +
         std::to_string(range.second) + "]";
    i++;
  }
  if (!SparseBitSetBytes().empty()) {
    if (!Ranges().empty()) {
      s += ",";
    }
    s += "bitset=" + std::to_string(SparseBitSetBytes().size()) + "b";
  }
  return s + "}";
}

CompressedSet& CompressedSet::operator=(CompressedSet&& other) noexcept {
  _sparse_bit_set_bytes = std::move(other._sparse_bit_set_bytes);
  _ranges = std::move(other._ranges);
  return *this;
}

bool CompressedSet::operator==(const CompressedSet& other) const {
  return _sparse_bit_set_bytes == other._sparse_bit_set_bytes &&
         _ranges == other._ranges;
}
bool CompressedSet::operator!=(const CompressedSet& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
