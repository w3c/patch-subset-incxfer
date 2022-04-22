#ifndef PATCH_SUBSET_CBOR_AXIS_SPACE_H_
#define PATCH_SUBSET_CBOR_AXIS_SPACE_H_

#include <optional>

#include "absl/container/flat_hash_map.h"
#include "cbor.h"
#include "common/status.h"
#include "hb-subset.h"
#include "patch_subset/cbor/axis_interval.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

/*
 * A range on a variation axis.
 *
 * See https://w3c.github.io/PFE/Overview.html#AxisSpace
 */
class AxisSpace {
 private:
  absl::flat_hash_map<hb_tag_t, std::vector<AxisInterval>> _space;

 public:
  AxisSpace() : _space() {}
  AxisSpace(const AxisSpace& other) = default;
  AxisSpace(AxisSpace&& other) noexcept : _space(std::move(other._space)) {}

  bool Has(hb_tag_t tag) const;
  void Clear(hb_tag_t tag);
  void AddInterval(hb_tag_t tag, const AxisInterval& interval);
  const std::vector<AxisInterval>& IntervalsFor(hb_tag_t tag) const;

  static StatusCode SetAxisSpaceField(
      cbor_item_t& map, int field_number,
      const std::optional<AxisSpace>& axis_space);
  static StatusCode GetAxisSpaceField(const cbor_item_t& map, int field_number,
                                      std::optional<AxisSpace>& out);
  static StatusCode Decode(const cbor_item_t& cbor_map, AxisSpace& out);
  StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  AxisSpace& operator=(AxisSpace&& other) noexcept;
  bool operator==(const AxisSpace& other) const;
  bool operator!=(const AxisSpace& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_AXIS_SPACE_H_
