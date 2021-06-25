#include "patch_subset/sparse_bit_set.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/status.h"
#include "hb.h"

using ::absl::string_view;

namespace patch_subset {

static const unsigned int kBitsPerByte = 8;

int TreeDepthFor(const hb_set_t& set) {
  hb_codepoint_t max_value = hb_set_get_max(&set);
  int depth = 1;
  hb_codepoint_t value = kBitsPerByte;
  while (value - 1 < max_value) {
    depth++;
    value *= kBitsPerByte;
  }
  return depth;
}

// Returns the maximum value that a tree of the given depth
// can encode.
unsigned int MaxValueForTreeOfDepth(unsigned int tree_depth) {
  int value = 1;
  for (unsigned int i = 0; i < tree_depth; i++) {
    value *= kBitsPerByte;
  }
  return value;
}

// Returns the number of values that can be encoded by
// the descendants of a single bit in the given layer
// of a tree with the given depth.
unsigned int ValuesPerBitForLayer(unsigned int layer, unsigned int tree_depth) {
  unsigned int tree_size = MaxValueForTreeOfDepth(tree_depth);
  for (unsigned int i = 0; i < layer; i++) {
    tree_size /= kBitsPerByte;
  }
  return tree_size / kBitsPerByte;
}

char MaskFor(unsigned int bit) { return 1 << bit; }

// Decodes a single layer that spans from start_index to end_index (inclusive)
// If this is not the last layer, it adds the indexes of the bytes in the next
// layer to layer_indices otherwise it populates the values in out.
unsigned int DecodeLayer(string_view sparse_bit_set, size_t start_index,
                         size_t end_index,
                         std::vector<unsigned int>* layer_indices, /* OUT */
                         hb_set_t* out /* OUT */) {
  bool has_more_layers = (layer_indices->size() < sparse_bit_set.size());
  size_t i = start_index;
  for (; i <= end_index && i < sparse_bit_set.size(); i++) {
    char byte = sparse_bit_set[i];
    for (unsigned int bit_index = 0; bit_index < kBitsPerByte; bit_index++) {
      char mask = MaskFor(bit_index);
      if (!(byte & mask)) {
        continue;
      }

      unsigned int index = (*layer_indices)[i] * kBitsPerByte + bit_index;
      if (has_more_layers) {
        layer_indices->push_back(index);
        continue;
      }
      hb_set_add(out, index);
    }
  }
  return i;
}

StatusCode SparseBitSet::Decode(string_view sparse_bit_set, hb_set_t* out) {
  if (!out) {
    return StatusCode::kInvalidArgument;
  }

  if (sparse_bit_set.empty()) {
    return StatusCode::kOk;
  }

  unsigned int byte_index = 0;
  std::vector<unsigned int> layer_indices;
  layer_indices.push_back(0);

  while (byte_index < sparse_bit_set.size()) {
    size_t end_index = layer_indices.size() - 1;
    if (end_index >= sparse_bit_set.size()) {
      return StatusCode::kInvalidArgument;
    }
    byte_index =
        DecodeLayer(sparse_bit_set, byte_index, end_index, &layer_indices, out);
  }

  return StatusCode::kOk;
}

void ExpandIfNeeded(unsigned int byte_index,
                    std::string* sparse_bit_set /* OUT */) {
  if (byte_index >= sparse_bit_set->size()) {
    sparse_bit_set->push_back(0);
  }
}

unsigned int NextIndex(hb_codepoint_t cp, unsigned int byte_index,
                       const std::vector<unsigned int>& byte_bases,
                       unsigned int values_per_byte) {
  if (cp >= byte_bases[byte_index] + values_per_byte) {
    // codepoint is past the current byte, move to the next one.
    return byte_index + 1;
  }
  return byte_index;
}

unsigned int EncodeLayer(const hb_set_t& set, unsigned int layer,
                         unsigned int tree_depth, unsigned int byte_index,
                         std::vector<unsigned int>* byte_bases, /* OUT */
                         std::string* sparse_bit_set /* OUT */) {
  unsigned int values_per_bit = ValuesPerBitForLayer(layer, tree_depth);
  unsigned int values_per_byte = values_per_bit * kBitsPerByte;

  // For each layer we iterate through every code point and determine which
  // bits need to be set in this layer. At the same time we compute the bases
  // for the next layer. A base is the starting value for the range of values
  // that a byte in the next layer covers
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    byte_index = NextIndex(cp, byte_index, *byte_bases, values_per_byte);
    ExpandIfNeeded(byte_index, sparse_bit_set);

    unsigned int bit_index = (cp - (*byte_bases)[byte_index]) / values_per_bit;
    char mask = MaskFor(bit_index);
    char byte = (*sparse_bit_set)[byte_index];

    if (byte & mask) {
      // Bit's already set, no action needed.
      continue;
    }
    // Setting this bit for the first time, record its base in the next
    // layer.
    (*sparse_bit_set)[byte_index] = byte | mask;
    if (values_per_bit > 1) {
      // Only compute bases if we're not in the last layer.
      byte_bases->push_back((cp / values_per_bit) * values_per_bit);
    }
  }

  // The next layer should start on a new byte.
  return byte_index + 1;
}

std::string SparseBitSet::Encode(const hb_set_t& set) {
  std::string sparse_bit_set;
  if (!hb_set_get_population(&set)) {
    return sparse_bit_set;
  }

  unsigned int depth = TreeDepthFor(set);

  unsigned int byte_index = 0;
  std::vector<unsigned int> byte_bases;
  byte_bases.push_back(0);
  for (unsigned int layer = 0; layer < depth; layer++) {
    byte_index = EncodeLayer(set, layer, depth, byte_index, &byte_bases,
                             &sparse_bit_set);
  }
  return sparse_bit_set;
}

}  // namespace patch_subset
