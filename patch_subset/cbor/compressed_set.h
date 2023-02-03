#ifndef PATCH_SUBSET_CBOR_COMPRESSED_SET_H_
#define PATCH_SUBSET_CBOR_COMPRESSED_SET_H_

#include <optional>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/range_list.h"

namespace patch_subset::cbor {

/*
 * // TODO: more doc.
 * A class that encodes a set of integers as the union of a bit set and a
 * list of integer ranges.
 * See https://w3c.github.io/PFE/Overview.html#CompressedSet
 */
class CompressedSet {
 private:
  std::optional<std::string> _sparse_bit_set_bytes;
  std::optional<range_vector> _ranges;

  // CBOR byte string, a SparseBitSet bit string.
  static const int kSparseBitSetFieldNumber = 0;

  // CBOR array (of ints), a CompressedList.
  static const int kSRangeDeltasFieldNumber = 1;

 public:
  CompressedSet();
  CompressedSet(const CompressedSet& other) = default;
  CompressedSet(CompressedSet&& other) noexcept;
  CompressedSet(absl::string_view sparse_bit_set_bytes,
                const range_vector& ranges);

  bool empty() const;
  static absl::StatusCode Decode(const cbor_item_t& cbor_map,
                                 CompressedSet& out);
  absl::StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  bool HasSparseBitSetBytes() const;
  CompressedSet& SetSparseBitSetBytes(const std::string& bytes);
  CompressedSet& ResetSparseBitSetBytes();
  const std::string& SparseBitSetBytes() const;

  bool HasRanges() const;
  CompressedSet& SetRanges(const range_vector& ranges);
  CompressedSet& AddRange(const range range);
  CompressedSet& ResetRanges();
  const range_vector& Ranges() const;

  static absl::StatusCode SetCompressedSetField(
      cbor_item_t& map, int field_number,
      const std::optional<CompressedSet>& compressed_set);
  static absl::StatusCode GetCompressedSetField(
      const cbor_item_t& map, int field_number,
      std::optional<CompressedSet>& out);

  // Returns a human readable version of this CompressedSet.
  std::string ToString() const;

  CompressedSet& operator=(CompressedSet&& other) noexcept;
  bool operator==(const CompressedSet& other) const;
  bool operator!=(const CompressedSet& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_COMPRESSED_SET_H_
