#include "patch_subset/sparse_bit_set.h"

#include <vector>

#include "absl/strings/string_view.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/bit_input_buffer.h"
#include "patch_subset/bit_output_buffer.h"

using ::absl::string_view;
using std::string;
using std::vector;

namespace patch_subset {

int TreeDepthFor(const hb_set_t& set, unsigned int bits_per_node) {
  hb_codepoint_t max_value = hb_set_get_max(&set);
  int depth = 1;
  hb_codepoint_t value = bits_per_node;
  while (value - 1 < max_value) {
    depth++;
    value *= bits_per_node;
  }
  return depth;
}

// Returns the number of values that can be encoded by the descendants of a
// single bit in the given layer of a tree with the given depth, using
// bits_per_node bits at each node.
//
// For example in layer 0 (root) of a tree of depth 3, with 2 bits per node,
// each bit (a node at level 1) represents 4 values (2 child nodes, each with
// 2 values).
unsigned int ValuesPerBitForLayer(unsigned int layer, unsigned int tree_depth,
                                  unsigned int bits_per_node) {
  unsigned int values = 1;  // Leaf nodes, each bit is 1 value;
  // Start at leaf layer, work up to target layer.
  for (unsigned int i = (int)tree_depth - 1; i > layer; i--) {
    values *= bits_per_node;
  }
  return values;
}

StatusCode SparseBitSet::Decode(string_view sparse_bit_set, hb_set_t* out) {
  if (!out) {
    return StatusCode::kInvalidArgument;
  }
  if (sparse_bit_set.empty()) {
    return StatusCode::kOk;
  }

  BitInputBuffer bits(sparse_bit_set);
  vector<unsigned int> pending_node_bases{0u};  // Root node.
  vector<unsigned int> next_level_node_bases;

  for (unsigned int level = 0; level < bits.Depth(); level++) {
    for (unsigned int current_node_base : pending_node_bases) {
      uint32_t current_node_bits;
      if (!bits.read(&current_node_bits)) {
        // Ran out of node bits.
        return StatusCode::kInvalidArgument;
      }
      for (unsigned int bit_index = 0; bit_index < bits.GetBranchFactor();
           bit_index++) {
        if (current_node_bits & 1u << bit_index) {
          if (level == bits.Depth() - 1) {
            hb_set_add(out, current_node_base + bit_index);
          } else {
            next_level_node_bases.push_back((current_node_base + bit_index) *
                                            bits.GetBranchFactor());
          }
        }
      }
    }
    pending_node_bases.swap(next_level_node_bases);
    next_level_node_bases.clear();
  }
  return StatusCode::kOk;
}

unsigned int EncodeLayer(const hb_set_t& set, unsigned int layer,
                         unsigned int tree_depth, unsigned int bits_per_node,
                         unsigned int current_node_base_index,
                         vector<unsigned int>& node_bases, /* OUT */
                         BitOutputBuffer& bit_buffer /* OUT */) {
  unsigned int values_per_bit =
      ValuesPerBitForLayer(layer, tree_depth, bits_per_node);
  unsigned int values_per_node = values_per_bit * bits_per_node;

  // For each layer we iterate through every code point and determine which
  // bits need to be set in this layer. At the same time we compute the
  // bases for the next layer. A base is the starting value for the range
  // of values that a node in the next layer covers.
  uint32_t current_node_mask = 0u;
  unsigned int current_node_base = node_bases[current_node_base_index];
  unsigned int current_node_max = current_node_base + values_per_node - 1;
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    if (cp > current_node_max) {
      // We are moving on to the next node, so write out the current node.
      bit_buffer.append(current_node_mask);
      // Reset for a new node.
      current_node_mask = 0u;
      current_node_base_index++;
      current_node_base = node_bases[current_node_base_index];
      current_node_max = current_node_base + values_per_node - 1;
    }

    // Figure out which sub-range (bit) cp falls in.
    unsigned int bit_index = (cp - current_node_base) / values_per_bit;
    uint32_t cp_mask = 1u << bit_index;

    // If this bit is already set, no action needed.
    if (!(current_node_mask & cp_mask)) {
      // We are setting this bit for the first time.
      current_node_mask |= cp_mask;
      // Record its base value in the next layer.
      if (values_per_bit > 1) {
        // Only compute bases if we're not in the last layer.
        node_bases.push_back(current_node_base + (bit_index * values_per_bit));
      }
    }
  }
  // Record the last node processed.
  bit_buffer.append(current_node_mask);

  // The next layer's first node.
  return current_node_base_index + 1;
}

string SparseBitSet::Encode(const hb_set_t& set, BranchFactor branch_factor) {
  if (!hb_set_get_population(&set)) {
    return "";
  }
  unsigned int bits_per_node = branch_factor;
  unsigned int depth = TreeDepthFor(set, bits_per_node);
  BitOutputBuffer bit_buffer(branch_factor, depth);

  unsigned int current_node_base_index =
      0;  // Next node to process is the root node.
  // Starting values of the encoding ranges of the nodes queued to be encoded.
  vector<unsigned int> node_bases;
  // Queue up the root node.
  node_bases.push_back(0);
  for (unsigned int layer = 0; layer < depth; layer++) {
    current_node_base_index =
        EncodeLayer(set, layer, depth, bits_per_node, current_node_base_index,
                    node_bases, bit_buffer);
  }
  return bit_buffer.to_string();
}

}  // namespace patch_subset
