#include "patch_subset/compressed_set.h"

#include <cmath>

#include "absl/types/optional.h"
#include "common/logging.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/sparse_bit_set.h"

typedef std::pair<hb_codepoint_t, hb_codepoint_t> range;
typedef std::vector<range> range_vector;

using ::absl::optional;

namespace patch_subset {

static const int kBitsPerByte = 8;

bool CompressedSet::IsEmpty(const CompressedSetProto& set) {
  return !set.sparse_bit_set().size() && !set.range_deltas_size();
}

StatusCode CompressedSet::Decode(const CompressedSetProto& set, hb_set_t* out) {
  if (set.range_deltas_size() % 2 != 0) {
    LOG(WARNING) << "Invalid compressed set proto. Wrong number of deltas.";
    return StatusCode::kInvalidArgument;
  }

  StatusCode result = SparseBitSet::Decode(set.sparse_bit_set(), out);
  if (result != StatusCode::kOk) {
    return result;
  }

  hb_codepoint_t last = 0;
  for (int i = 0; i < set.range_deltas_size(); i += 2) {
    hb_codepoint_t to_start = set.range_deltas(i);
    hb_codepoint_t to_end = set.range_deltas(i + 1);
    hb_codepoint_t range_start = last + to_start;
    hb_codepoint_t range_end = range_start + to_end;
    hb_set_add_range(out, range_start, range_end);
    last = range_end;
  }
  return StatusCode::kOk;
}

range_vector ToRanges(const hb_set_t& set) {
  range_vector out;

  range current(HB_SET_VALUE_INVALID, 0);

  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    if (current.first == HB_SET_VALUE_INVALID) {
      current.first = cp;
      current.second = cp;
      continue;
    }

    if (cp == current.second + 1) {
      // Continue the current range.
      current.second = cp;
      continue;
    }

    // Switchting to a new range.
    out.push_back(current);
    current.first = cp;
    current.second = cp;
  }

  // Pack the current range.
  if (current.first != HB_SET_VALUE_INVALID) {
    out.push_back(current);
  }

  return out;
}

int VariableIntegerEncodedSize(int value) {
  int size = 0;
  do {
    value /= 128;  // In variable encoding 7 of the 8 bits of each byte are used
                   // to encode value.
    size++;
  } while (value > 0);
  return size;
}

int RangeEncodedSize(const range& last_range, const range& range) {
  // For begin and end estimate the number of bytes needed to encode them using
  // variable length encoding.
  return VariableIntegerEncodedSize(range.first - last_range.second) +
         VariableIntegerEncodedSize(range.second - range.first);
}

int BitSetEncodedSize(const range& range,
                      const optional<::range>& previous_range,
                      const optional<::range>& next_range) {
  // For bit set encoding we can estimate byte usage by assuming we need 1 bit
  // per value in the range. (This ignores the interior nodes that result from
  // the leaf nodes)
  int byte_count = static_cast<int>(ceil((range.second - range.first + 1) /
                                         static_cast<double>(kBitsPerByte)));
  if (previous_range &&
      (*previous_range).second / kBitsPerByte == range.first / kBitsPerByte) {
    // If the previous range shares the first byte of this range then we
    // shouldn't count it.
    byte_count--;
  }

  if (next_range &&
      (*next_range).first / kBitsPerByte == range.second / kBitsPerByte) {
    // If the next range shares the last byte of this range then we shouldn't
    // count it.
    byte_count--;
  }
  return byte_count;
}

void StrategyFor(const range& range, const optional<::range>& previous_range,
                 const optional<::range>& next_range,
                 hb_set_t* sparse_set, /* OUT */
                 range_vector* output_ranges /* OUT */) {
  ::range default_range(0, 0);
  ::range* last_output_range = &default_range;
  if (!output_ranges->empty()) {
    last_output_range = &output_ranges->back();
  }

  if (range.first != range.second &&
      RangeEncodedSize(*last_output_range, range) <=
          BitSetEncodedSize(range, previous_range, next_range)) {
    output_ranges->push_back(range);
  } else {
    hb_set_add_range(sparse_set, range.first, range.second);
  }
}

void EncodingStrategy(const hb_set_t& set, hb_set_t* sparse_set, /* OUT */
                      range_vector* output_ranges /* OUT */) {
  range_vector input_ranges(ToRanges(set));
  for (unsigned int i = 0; i < input_ranges.size(); i++) {
    optional<::range> previous_range =
        i > 0 ? optional<::range>(input_ranges[i - 1]) : optional<::range>();
    optional<::range> next_range = i + 1 < input_ranges.size()
                                       ? optional<::range>(input_ranges[i + 1])
                                       : optional<::range>();
    StrategyFor(input_ranges[i], previous_range, next_range, sparse_set,
                output_ranges);
  }
}

void CompressedSet::Encode(const hb_set_t& set, CompressedSetProto* out) {
  // TODO(garretrieger): implement further compression of the sparse bit set, by
  // removing
  //                the numeric space used for any encoded ranges. See java
  //                implementation HybridSerializer for details.
  range_vector ranges;
  hb_set_unique_ptr sparse_set = make_hb_set();
  EncodingStrategy(set, sparse_set.get(), &ranges);

  // Encode sparse bit set.
  out->set_sparse_bit_set(SparseBitSet::Encode(*sparse_set));

  // Encode range deltas.
  int last = 0;
  for (const auto& range : ranges) {
    out->add_range_deltas(range.first - last);
    out->add_range_deltas(range.second - range.first);
    last = range.second;
  }
}

}  // namespace patch_subset
