#include "patch_subset/cbor/axis_interval.h"

#include "absl/strings/str_cat.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;

AxisInterval::AxisInterval() : _start(std::nullopt), _end(std::nullopt) {}

AxisInterval::AxisInterval(AxisInterval&& other) noexcept
    : _start(other._start), _end(other._end) {}

AxisInterval::AxisInterval(float point) : _start(point), _end(std::nullopt) {}

AxisInterval::AxisInterval(float start, float end) : _start(start), _end(end) {}

bool AxisInterval::IsPoint() const {
  return _start && (!_end || *_start == *_end);
}

bool AxisInterval::IsValid() const {
  if (_start) {
    return !_end || *_start <= *_end;
  }
  return !_end;
}

Status AxisInterval::Decode(const cbor_item_t& cbor_map, AxisInterval& out) {
  if (!cbor_isa_map(&cbor_map)) {
    return absl::InvalidArgumentError("not a map.");
  }

  AxisInterval result;

  Status sc =
      CborUtils::GetFloatField(cbor_map, kStartFieldNumber, result._start);
  if (!sc.ok()) {
    return sc;
  }

  sc = CborUtils::GetFloatField(cbor_map, kEndFieldNumber, result._end);
  if (!sc.ok()) {
    return sc;
  }

  if (!result.IsValid()) {
    return absl::InvalidArgumentError("Invalid axis interval.");
  }

  out = std::move(result);
  return sc;
}

Status AxisInterval::Encode(cbor_item_unique_ptr& map_out) const {
  int size = (_start ? 1 : 0) + (_end ? 1 : 0);
  cbor_item_unique_ptr map = make_cbor_map(size);

  if (!IsValid()) {
    return absl::InvalidArgumentError("Invalid axis interval.");
  }

  Status sc = CborUtils::SetFloatField(*map, kStartFieldNumber, _start);
  if (!sc.ok()) {
    return sc;
  }

  if (!IsPoint()) {
    sc = CborUtils::SetFloatField(*map, kEndFieldNumber, _end);
    if (!sc.ok()) {
      return sc;
    }
  }

  map_out.swap(map);
  return absl::OkStatus();
}

bool AxisInterval::HasStart() const { return bool(_start); }

AxisInterval& AxisInterval::SetStart(float value) {
  _start.emplace(value);
  return *this;
}

AxisInterval& AxisInterval::ResetStart() {
  _start.reset();
  return *this;
}

float AxisInterval::Start() const { return *_start; }

bool AxisInterval::HasEnd() const { return bool(_start || _end); }

AxisInterval& AxisInterval::SetEnd(float value) {
  _end.emplace(value);
  return *this;
}

AxisInterval& AxisInterval::ResetEnd() {
  _end.reset();
  return *this;
}

float AxisInterval::End() const {
  if (IsPoint()) {
    return Start();
  }
  return *_end;
}

std::string AxisInterval::ToString() const {
  if (_start && _end) {
    return absl::StrCat("[", *_start, ", ", *_end, "]");
  }

  if (_start && !_end) {
    return absl::StrCat("[", *_start, ", ", *_start, "]");
  }

  return "[]";
}

AxisInterval& AxisInterval::operator=(AxisInterval&& other) noexcept {
  _start = other._start;
  _end = other._end;
  return *this;
}

bool AxisInterval::operator==(const AxisInterval& other) const {
  if (IsPoint() && other.IsPoint()) {
    return Start() == other.Start();
  }

  return _start == other._start && _end == other._end;
}

bool AxisInterval::operator!=(const AxisInterval& other) const {
  return !(*this == other);
}

}  // namespace patch_subset::cbor
