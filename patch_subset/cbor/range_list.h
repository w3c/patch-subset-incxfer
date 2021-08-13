#ifndef PATCH_SUBSET_CBOR_RANGE_LIST_H_
#define PATCH_SUBSET_CBOR_RANGE_LIST_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

typedef std::pair<uint32_t, uint32_t> range;
typedef std::vector<range> range_vector;

/*
 * Convert a range list (sorted), e.g. [1..5], [9..13], [20,25], into a sorted
 * integer list which is encoded by the IntegerList class.
 */
class RangeList {
 public:
  // Interpret a CBOR bytestring as a compressed range list, of sorted values.
  static StatusCode Decode(const cbor_item_t& array, range_vector& out);

  // Create a compressed list given a sorted list of ranges.
  static StatusCode Encode(const range_vector& ranges,
                           cbor_item_unique_ptr& bytestring_out);

  static StatusCode SetRangeListField(
      cbor_item_t& map, int field_number,
      const std::optional<range_vector>& int_list);
  static StatusCode GetRangeListField(const cbor_item_t& map, int field_number,
                                      std::optional<range_vector>& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_RANGE_LIST_H_
