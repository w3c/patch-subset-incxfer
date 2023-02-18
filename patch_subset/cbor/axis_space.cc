#include "patch_subset/cbor/axis_space.h"

#include <optional>

#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;

bool AxisSpace::Empty() const { return _space.empty(); }

bool AxisSpace::Has(hb_tag_t tag) const { return _space.contains(tag); }

void AxisSpace::Clear(hb_tag_t tag) { _space.erase(tag); }

void AxisSpace::AddInterval(hb_tag_t tag, const AxisInterval& interval) {
  _space[tag].push_back(interval);
}

const std::vector<AxisInterval>& AxisSpace::IntervalsFor(hb_tag_t tag) const {
  static std::vector<AxisInterval> empty;

  if (auto it = _space.find(tag); it != _space.end()) {
    return it->second;
  }

  return empty;
}

Status AxisSpace::SetAxisSpaceField(
    cbor_item_t& map, int field_number,
    const std::optional<AxisSpace>& axis_space) {
  if (!axis_space.has_value()) {
    return absl::OkStatus();  // Nothing to do.
  }
  cbor_item_unique_ptr field_value = empty_cbor_ptr();
  Status sc = axis_space.value().Encode(field_value);
  if (!sc.ok()) {
    return sc;
  }
  return CborUtils::SetField(map, field_number, move_out(field_value));
}

Status AxisSpace::GetAxisSpaceField(const cbor_item_t& map, int field_number,
                                    std::optional<AxisSpace>& out) {
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(map, field_number, field);
  if (absl::IsNotFound(sc)) {
    out.reset();
    return absl::OkStatus();
  } else if (!sc.ok()) {
    return sc;
  }
  AxisSpace results;
  sc = Decode(*field, results);
  if (!sc.ok()) {
    return sc;
  }
  out.emplace(results);
  return absl::OkStatus();
}

Status AxisSpace::Decode(const cbor_item_t& cbor_map, AxisSpace& out) {
  if (!cbor_isa_map(&cbor_map)) {
    return absl::InvalidArgumentError("Not a map.");
  }

  Status sc;
  AxisSpace result;

  size_t map_size = cbor_map_size(&cbor_map);
  cbor_pair* pairs = cbor_map_handle(&cbor_map);
  for (unsigned i = 0; i < map_size; i++) {
    cbor_item_t* tag_str = pairs[i].key;
    if (!cbor_isa_bytestring(tag_str) || cbor_bytestring_length(tag_str) != 4) {
      return absl::InvalidArgumentError("Not a byte string of length > 4.");
    }

    hb_tag_t tag = hb_tag_from_string(
        reinterpret_cast<char*>(cbor_bytestring_handle(tag_str)), 4);

    cbor_item_t* intervals = pairs[i].value;
    if (!cbor_isa_array(intervals)) {
      return absl::InvalidArgumentError("Not an array.");
    }

    size_t intervals_length = cbor_array_size(intervals);
    cbor_item_t** intervals_array = cbor_array_handle(intervals);
    for (unsigned i = 0; i < intervals_length; i++) {
      AxisInterval interval;
      if (!(sc = interval.Decode(*intervals_array[i], interval)).ok()) {
        return sc;
      }

      result._space[tag].push_back(std::move(interval));
    }
  }

  out = std::move(result);
  return absl::OkStatus();
}

Status AxisSpace::Encode(cbor_item_unique_ptr& map_out) const {
  cbor_item_unique_ptr map = make_cbor_map(_space.size());
  Status sc;

  for (const auto& i : _space) {
    char tag_str[4];
    hb_tag_to_string(i.first, tag_str);
    absl::string_view key(tag_str, 4);

    cbor_item_unique_ptr cbor_key = wrap_cbor_item(CborUtils::EncodeBytes(key));
    cbor_item_unique_ptr intervals_array = make_cbor_array(i.second.size());

    for (const auto& interval : i.second) {
      cbor_item_unique_ptr cbor_interval = empty_cbor_ptr();
      if (!(sc = interval.Encode(cbor_interval)).ok()) {
        return sc;
      }

      if (!cbor_array_push(intervals_array.get(), cbor_interval.get())) {
        return absl::InternalError("cbor_array_push failed.");
      }
    }

    bool ok = cbor_map_add(
        map.get(),
        cbor_pair{.key = cbor_key.get(), .value = intervals_array.get()});

    if (!ok) {
      return absl::InternalError("cbor_map_add failed.");
    }
  }

  map_out.swap(map);
  return absl::OkStatus();
}

std::string AxisSpace::ToString() const {
  std::string result = "";
  for (auto const& e : _space) {
    result += e.first;
    result += ": [";
    int j = 0;
    for (auto const& i : e.second) {
      if (j++ > 0) {
        result += ", ";
      }
      result += i.ToString();
    }
    result += "]";
  }
  return result;
}

AxisSpace& AxisSpace::operator=(AxisSpace&& other) noexcept {
  _space = std::move(other._space);
  return *this;
}

bool AxisSpace::operator==(const AxisSpace& other) const {
  return _space == other._space;
}

bool AxisSpace::operator!=(const AxisSpace& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
