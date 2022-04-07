#ifndef PATCH_SUBSET_CBOR_AXIS_INTERVAL_H_
#define PATCH_SUBSET_CBOR_AXIS_INTERVAL_H_

#include <optional>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

/*
 * A range on a variation axis.
 *
 * See https://w3c.github.io/PFE/Overview.html#AxisInterval
 */
class AxisInterval {
 private:
  std::optional<float> _start;
  std::optional<float> _end;

  static const int kStartFieldNumber = 0;  // CBOR floating point number.
  static const int kEndFieldNumber = 1;    // CBOR floating point number.

 public:
  AxisInterval();
  AxisInterval(const AxisInterval& other) = default;
  AxisInterval(AxisInterval&& other) noexcept;
  AxisInterval(float point);
  AxisInterval(float start, float end);

  bool IsPoint() const;
  bool IsValid() const;

  static StatusCode Decode(const cbor_item_t& cbor_map, AxisInterval& out);
  StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  bool HasStart() const;
  AxisInterval& SetStart(float value);
  AxisInterval& ResetStart();
  float Start() const;

  bool HasEnd() const;
  AxisInterval& SetEnd(float value);
  AxisInterval& ResetEnd();
  float End() const;

  std::string ToString() const;

  AxisInterval& operator=(AxisInterval&& other) noexcept;
  bool operator==(const AxisInterval& other) const;
  bool operator!=(const AxisInterval& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_AXIS_INTERVAL_H_
