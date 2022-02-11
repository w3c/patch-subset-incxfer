#include "patch_subset/sparse_bit_set.h"

#include <map>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/bit_input_buffer.h"
#include "patch_subset/bit_output_buffer.h"

using ::absl::string_view;
using std::map;
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
  unsigned int bits_per_node = bits.GetBranchFactor();
  unsigned int tree_height = bits.Depth();
  // At each level, this is the number of leaf values a node covers.
  unsigned int leaf_node_size = bits_per_node;
  for (unsigned int i = 1; i < tree_height; i++) {
    leaf_node_size *= bits_per_node;
  }
  // At each level, to get from node_base to the values at the leaf level,
  // multiply by this. For example in a bf=4 d=4 tree, at level 1, the node
  // with node_base 2 covers final leaf values starting at 2 * 16.
  unsigned int node_base_factor = leaf_node_size / bits_per_node;
  vector<unsigned int> node_bases{0u};  // Root node.
  vector<unsigned int> filled_node_bases;
  vector<unsigned int> filled_node_sizes;
  vector<unsigned int> next_level_node_bases;

  for (unsigned int level = 0; level < tree_height; level++) {
    for (unsigned int node_base : node_bases) {
      // This is a normal node so read a node's worth of bits.
      uint32_t current_node_bits;
      if (!bits.read(&current_node_bits)) {
        // Ran out of node bits.
        return StatusCode::kInvalidArgument;
      }
      if (current_node_bits == 0u) {
        // This is a completely filled node encoded as a zero!
        unsigned int leaf_node_base = node_base * node_base_factor;
        hb_set_add_range(out, leaf_node_base,
                         leaf_node_base + leaf_node_size - 1);
      } else {
        // It's a normally encoded node.
        for (unsigned int bit_index = 0u; bit_index < bits_per_node;
             bit_index++) {
          if (current_node_bits & (1u << bit_index)) {
            if (level == tree_height - 1) {
              hb_set_add(out, node_base + bit_index);
            } else {
              next_level_node_bases.push_back((node_base + bit_index) *
                                              bits_per_node);
            }
          }
        }
      }
    }
    leaf_node_size /= bits_per_node;
    node_base_factor /= bits_per_node;
    node_bases.swap(next_level_node_bases);
    next_level_node_bases.clear();
  }
  return StatusCode::kOk;
}

/*
 * Determines which nodes are completely filled, and thus should be encoded with
 * a zero. Leaf nodes are never marked as filled - writing all 0s instead of all
 * ones would not save any bytes. So the length of the array is the number of
 * nodes one level above the leaf level. For a given codepoint CP, the value
 * stored at index CP / (bits_per_node * bits_per_node). The value will be the
 * tree depth (0 for root) at which the node is first completely filled, and
 * thus should be encoded as a zero. The value will be greater than the tree
 * height when no filled nodes exist for these codepoints.
 */
map<uint32_t, uint8_t> FindFilledNodes(const hb_set_t& set,
                                       unsigned int bits_per_node,
                                       unsigned int tree_height) {
  map<uint32_t, uint8_t> filled_levels;
  if (tree_height < 2) {
    return filled_levels;
  }
  // "Twigs" are nodes one layer above the leaves.
  // Find the completely filled twig nodes. Layer tree_height - 2.
  unsigned int twig_size = bits_per_node * bits_per_node;
  unsigned int prev_cp = UINT32_MAX - 1;
  unsigned int seq_len = 0;
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    if (cp == prev_cp + 1) {
      seq_len++;
    } else {
      seq_len = 1;
    }
    if ((cp + 1) % twig_size == 0) {
      if (seq_len == twig_size) {
        filled_levels[cp / twig_size] = tree_height - 2;
      }
      seq_len = 0;
    }
    prev_cp = cp;
  }
  if (filled_levels.empty()) {
    return filled_levels;
  }

  // Now work our way up the layers, "merging" filled nodes by decrementing
  // their filled-at number. Start processing at the layer above the twigs.
  uint32_t node_size = bits_per_node;  // Number to twigs to consider as a node.
  for (int layer = (int)tree_height - 3; layer >= 0; layer--) {
    uint8_t target_level = layer + 1;
    uint32_t prev_twig = UINT32_MAX - 1;
    seq_len = 0;
    unsigned int num_merged_nodes = 0;
    for (auto e : filled_levels) {
      uint32_t twig = e.first;
      uint8_t filled_level = e.second;
      if (twig == prev_twig + 1 && filled_level == target_level) {
        seq_len++;  // Continue a good sequence.
      } else if (filled_level == target_level) {
        seq_len = 1;  // Start a possible new sequence.
      } else {
        seq_len = 0;  // Can not be part of a sequence.
      }
      if ((twig + 1) % node_size == 0) {
        if (seq_len == node_size) {
          for (uint32_t i = twig - node_size + 1; i <= twig; i++) {
            filled_levels.find(i)->second = layer;  // Increment to next level.
          }
          num_merged_nodes++;
        }
        seq_len = 0;
      }
      prev_twig = twig;
    }
    if (num_merged_nodes < bits_per_node) {
      break;  // No further merges are possible.
    }
    node_size *= bits_per_node;
  }
  return filled_levels;
}

enum EncodeState {
  START,
  BUILDING_NORMAL_NODE,
  SKIPPING_FILLED_NODE,
  END,
  ERROR,
};

enum EncodeSymbolType {
  NEW_NORMAL_NODE,
  EXISTING_NORMAL_NODE,
  NEW_FILLED_NODE,
  EXISTING_FILLED_NODE,
  END_OF_VALUES,
  INVALID,
};

struct EncodeSymbol {
  EncodeSymbolType type;
  uint32_t cp;
};

static const uint32_t kInvalidCp = UINT32_MAX;
static const EncodeSymbol kEndOfValues =
    EncodeSymbol{END_OF_VALUES, kInvalidCp};

struct EncodeContext {
  const unsigned int layer;
  const unsigned int bits_per_node;
  const unsigned int tree_height;
  const uint32_t twig_size;
  const uint32_t values_per_bit;
  const uint32_t node_size;
  const map<uint32_t, uint8_t>& filled_levels;
  const vector<unsigned int>& node_bases;
  int next_node_base;
  uint32_t node_base;
  uint32_t node_max;
  uint32_t node_mask;
  uint32_t filled_max;
  vector<unsigned int>& next_node_bases; /* OUT */
  BitOutputBuffer& bit_buffer;           /* OUT */
};

static EncodeSymbolType OverrideIfFilled(uint32_t cp,
                                         const EncodeContext& context) {
  uint32_t twig = cp / context.twig_size;
  if (context.filled_levels.count(twig)) {
    uint8_t filled_level = context.filled_levels.at(twig);
    if (context.layer == filled_level) {
      return NEW_FILLED_NODE;
    } else if (context.layer > filled_level) {
      return EXISTING_FILLED_NODE;
    }
  }
  return NEW_NORMAL_NODE;
}

static void ParseCodepoint(uint32_t cp, EncodeState state,
                           const EncodeContext& context,
                           EncodeSymbol& symbol /* OUT */) {
  symbol.cp = cp;
  switch (state) {
    case START:
      symbol.type = OverrideIfFilled(cp, context);
      break;
    case BUILDING_NORMAL_NODE:
      if (cp <= context.node_max) {
        symbol.type = EXISTING_NORMAL_NODE;
      } else {
        symbol.type = OverrideIfFilled(cp, context);
      }
      break;
    case SKIPPING_FILLED_NODE:
      if (cp <= context.filled_max) {
        symbol.type = EXISTING_FILLED_NODE;  // Keep skipping.
      } else {
        symbol.type = OverrideIfFilled(cp, context);
      }
      break;
    case END:
    case ERROR:
      // No more values should happen while in the END state.
      symbol.cp = kInvalidCp;
      symbol.type = INVALID;
  }
}

static void StartFilledNode(EncodeContext& context) {
  uint32_t node_base = context.node_bases[context.next_node_base++];
  context.bit_buffer.append(0u);
  context.filled_max = node_base + context.node_size - 1;
}

static void SkipExistingFilledNode(uint32_t cp, EncodeContext& context) {
  uint32_t twig = cp / context.twig_size;
  // Scan to the right across all applicable filled twigs.
  do {
    uint8_t filled_depth = context.filled_levels.at(twig);
    // # of twigs covered by this filled node depends on its level.
    uint32_t twig_size = 1;
    for (int layer = context.tree_height - 2; layer >filled_depth; layer--) {
      twig_size *= context.bits_per_node;
    }
    // Advance 1 past this filled node.
    twig += twig_size;
    // Did we land on another filled node?
  } while (context.filled_levels.count(twig) &&
           context.filled_levels.at(twig) < context.layer);
  context.filled_max = (twig * context.twig_size) - 1;
}

static void EndNormalNode(EncodeContext& context) {
  context.bit_buffer.append(context.node_mask);
  // Reset context.
  context.node_mask = 0u;
  context.node_base = kInvalidCp;
  context.node_max = kInvalidCp;
  context.filled_max = kInvalidCp;
}

static void UpdateNodeBit(uint32_t cp, EncodeContext& context) {
  // Figure out which sub-range (bit) cp falls in.
  unsigned int bit_index = (cp - context.node_base) / context.values_per_bit;
  uint32_t cp_mask = 1u << bit_index;

  // If this bit is already set, no action needed.
  if (!(context.node_mask & cp_mask)) {
    // We are setting this bit for the first time.
    context.node_mask |= cp_mask;
    // Record its base value in the next layer.
    if (context.values_per_bit > 1) {
      // Only compute bases if we're not in the last/leaf layer.
      context.next_node_bases.push_back(context.node_base +
                                        (bit_index * context.values_per_bit));
    }
  }
}

static void StartNewNormalNode(uint32_t cp, EncodeContext& context) {
  context.node_base = context.node_bases[context.next_node_base++];
  context.node_max = context.node_base + context.node_size - 1;
  context.filled_max = kInvalidCp;
  UpdateNodeBit(cp, context);
}

static void UpdateNormalNode(uint32_t cp, EncodeContext& context) {
  UpdateNodeBit(cp, context);
}

static EncodeState UpdateState(EncodeState state, const EncodeSymbol& input,
                               EncodeContext& context) {
  if (input.type == INVALID || state == ERROR || state == END) {
    return ERROR;
  }
  switch (state) {
    case START:
      switch (input.type) {
        case NEW_NORMAL_NODE:
          StartNewNormalNode(input.cp, context);
          return BUILDING_NORMAL_NODE;
        case NEW_FILLED_NODE:
          StartFilledNode(context);
          return SKIPPING_FILLED_NODE;
        case EXISTING_FILLED_NODE:
          SkipExistingFilledNode(input.cp, context);
          return SKIPPING_FILLED_NODE;
        default:
          return ERROR;
      }
    case BUILDING_NORMAL_NODE: {
      switch (input.type) {
        case NEW_NORMAL_NODE:
          EndNormalNode(context);
          StartNewNormalNode(input.cp, context);
          return BUILDING_NORMAL_NODE;
        case EXISTING_NORMAL_NODE:
          // Stay in state BUILDING_NORMAL_NODE.
          UpdateNormalNode(input.cp, context);
          return BUILDING_NORMAL_NODE;
        case NEW_FILLED_NODE:
          EndNormalNode(context);
          StartFilledNode(context);
          return SKIPPING_FILLED_NODE;
        case EXISTING_FILLED_NODE:
          EndNormalNode(context);
          SkipExistingFilledNode(input.cp, context);
          return SKIPPING_FILLED_NODE;
        case END_OF_VALUES:
          EndNormalNode(context);
          return END;
        default:
          return ERROR;
      }
    }
    case SKIPPING_FILLED_NODE:
      switch (input.type) {
        case NEW_NORMAL_NODE:
          StartNewNormalNode(input.cp, context);
          return BUILDING_NORMAL_NODE;
        case NEW_FILLED_NODE:
          // Stay in state SKIPPING_FILLED_NODE.
          StartFilledNode(context);
          return SKIPPING_FILLED_NODE;
        case EXISTING_FILLED_NODE:
          // Ignore value. Stay in state SKIPPING_FILLED_NODE.
          return SKIPPING_FILLED_NODE;
        case END_OF_VALUES:
          return END;
        default:
          return ERROR;
      }
    default:
      return ERROR;
  }
}

void EncodeLayer(const hb_set_t& set, unsigned int layer,
                 unsigned int tree_height, unsigned int bits_per_node,
                 const map<uint32_t, uint8_t>& filled_levels,
                 const vector<unsigned int>& node_bases,
                 vector<unsigned int>& next_node_bases, /* OUT */
                 BitOutputBuffer& bit_buffer /* OUT */) {
  unsigned int values_per_bit =
      ValuesPerBitForLayer(layer, tree_height, bits_per_node);
  uint32_t node_size = values_per_bit * bits_per_node;
  unsigned int twig_size = bits_per_node * bits_per_node;
  EncodeContext context{layer,         bits_per_node,   tree_height,
                        twig_size,     values_per_bit,  node_size,
                        filled_levels, node_bases,      0,
                        kInvalidCp,    kInvalidCp,      0u,
                        kInvalidCp,    next_node_bases, bit_buffer};
  EncodeState state = START;
  EncodeSymbol input{INVALID, kInvalidCp};
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    ParseCodepoint(cp, state, context, input);
    state = UpdateState(state, input, context);
  }
  UpdateState(state, kEndOfValues, context);
}

string SparseBitSet::Encode(const hb_set_t& set, BranchFactor branch_factor) {
  if (!hb_set_get_population(&set)) {
    return "";
  }
  //  unsigned int bits_per_node = branch_factor;
  unsigned int tree_height = TreeDepthFor(set, branch_factor);
  // Determine which nodes are completely filled; encode them with zero.
  map<uint32_t, uint8_t> filled_levels =
      FindFilledNodes(set, branch_factor, tree_height);
  BitOutputBuffer bit_buffer(branch_factor, tree_height);

  // Starting values of the encoding ranges of the nodes queued to be encoded.
  // Queue up the root node.
  vector<unsigned int> node_bases(1, 0);
  vector<unsigned int> next_node_bases;
  for (unsigned int layer = 0; layer < tree_height; layer++) {
    EncodeLayer(set, layer, tree_height, branch_factor, filled_levels,
                node_bases, next_node_bases, bit_buffer);
    if (next_node_bases.empty()) {
      break;  // Filled nodes mean nothing left to encode.
    }
    node_bases.swap(next_node_bases);
    next_node_bases.clear();
  }
  return bit_buffer.to_string();
}

}  // namespace patch_subset
