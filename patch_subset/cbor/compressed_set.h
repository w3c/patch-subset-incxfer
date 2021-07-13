#ifndef PATCH_SUBSET_CBOR_COMPRESSED_SET_H_
#define PATCH_SUBSET_CBOR_COMPRESSED_SET_H_

#include <optional>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/compressed_range_list.h"

namespace patch_subset::cbor {

using std::optional;
using std::string;

/*
 * // TODO: more doc.
 * A class that encodes a set of integers as the union of a bit set and a
 * list of integer ranges.
 * See https://w3c.github.io/PFE/Overview.html#CompressedSet
 */
class CompressedSet {
 private:
  optional<string> _sparse_bit_set_bytes;
  optional<range_vector> _ranges;

  // CBOR byte string, a SparseBitSet bit string.
  static const int kSparseBitSetFieldNumber = 0;

  // CBOR array (of ints), a CompressedList.
  static const int kSRangeDeltasFieldNumber = 1;

 public:
  CompressedSet();
  CompressedSet(string_view sparse_bit_set_bytes, const range_vector& ranges);

  static StatusCode Decode(const cbor_item_t& cbor_map, CompressedSet& out);

  bool HasSparseBitSetBytes() const;
  CompressedSet& SetSparseBitSetBytes(const string& bytes);
  CompressedSet& ResetSparseBitSetBytes();
  string SparseBitSetBytes() const;

  bool HasRanges() const;
  CompressedSet& SetRanges(range_vector ranges);
  CompressedSet& ResetRanges();
  range_vector Ranges() const;

  StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  static StatusCode SetCompressedSetField(
      cbor_item_t& map, int field_number,
      const optional<CompressedSet>& compressed_set);
  static StatusCode GetCompressedSetField(const cbor_item_t& map,
                                          int field_number,
                                          optional<CompressedSet>& out);

  bool operator==(const CompressedSet& other) const;
  bool operator!=(const CompressedSet& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_COMPRESSED_SET_H_
