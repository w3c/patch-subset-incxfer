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

int TreeDepthFor(const hb_set_t& set, uint32_t bits_per_node) {
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
uint32_t ValuesPerBitForLayer(uint32_t layer, uint32_t tree_depth,
                              uint32_t bits_per_node) {
  uint32_t values = 1;  // Leaf nodes, each bit is 1 value;
  // Start at leaf layer, work up to target layer.
  for (uint32_t i = (int)tree_depth - 1; i > layer; i--) {
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
  BranchFactor branch_factor = bits.GetBranchFactor();
  uint32_t tree_height = bits.Depth();

  // Enforce upper limits on tree sizes.
  // We only need to encode the 32 bit range 0x0 .. 0xFFFFFFFF.
  if (tree_height > kBFMaxDepth[branch_factor]) {
    return StatusCode::kInvalidArgument;
  }

  // At each level, this is the number of leaf values a node covers.
  uint32_t leaf_node_size = kBFNodeSize[branch_factor];
  for (uint32_t i = 1; i < tree_height; i++) {
    leaf_node_size *= kBFNodeSize[branch_factor];
  }
  // At each level, to get from node_base to the values at the leaf level,
  // multiply by this. For example in a BF=4 D=4 tree, at level 1, the node
  // with node_base 2 covers final leaf values starting at 2 * 16.
  uint32_t node_base_factor = leaf_node_size / kBFNodeSize[branch_factor];
  vector<uint32_t> node_bases{0u};  // Root node.
  vector<uint32_t> filled_node_bases;
  vector<uint32_t> filled_node_sizes;
  vector<uint32_t> next_level_node_bases;

  for (uint32_t level = 0; level < tree_height; level++) {
    for (uint32_t node_base : node_bases) {
      // This is a normal node so read a node's worth of bits.
      uint32_t current_node_bits;
      if (!bits.read(&current_node_bits)) {
        // Ran out of node bits.
        return StatusCode::kInvalidArgument;
      }
      if (current_node_bits == 0u) {
        // This is a completely filled node encoded as a zero!
        uint32_t leaf_node_base = node_base * node_base_factor;
        hb_set_add_range(out, leaf_node_base,
                         leaf_node_base + leaf_node_size - 1);
      } else {
        // It's a normally encoded node.
        for (uint32_t bit_index = 0u; bit_index < kBFNodeSize[branch_factor];
             bit_index++) {
          if (current_node_bits & (1u << bit_index)) {
            if (level == tree_height - 1) {
              hb_set_add(out, node_base + bit_index);
            } else {
              next_level_node_bases.push_back((node_base + bit_index) *
                                              kBFNodeSize[branch_factor]);
            }
          }
        }
      }
    }
    leaf_node_size /= kBFNodeSize[branch_factor];
    node_base_factor /= kBFNodeSize[branch_factor];
    node_bases.swap(next_level_node_bases);
    next_level_node_bases.clear();
  }
  return StatusCode::kOk;
}

static void AdvanceToCp(uint32_t prev_cp, uint32_t cp,
                        uint32_t empty_leaves[BF32 + 1] /* OUT */) {
  if ((cp < kBFNodeSize[BF2]) || (cp - prev_cp < kBFNodeSize[BF2])) {
    return;
  }
  uint32_t first_missing = prev_cp + 1;
  // Count skipped over nodes, if any.
  for (BranchFactor bf : {BF2, BF4, BF8, BF32}) {
    // Find start of node at least 1 after last cp (first missing value).
    uint32_t remainder = first_missing % kBFNodeSize[bf];
    uint32_t start = remainder ? first_missing + (kBFNodeSize[bf] - remainder)
                               : first_missing;
    // Find start of node containing current value - 1 (last missing value).
    uint32_t end = cp - (cp % kBFNodeSize[bf]);
    if (end > start) {
      empty_leaves[bf] += (end - start) / kBFNodeSize[bf];
    }
  }
}

/*
 * These values were chosen to match the tree sizes seen in a combination of
 * uniform random and usage-frequency weighted random sets.
 */
uint32_t EstimateTreeSize(uint32_t num_leaf_nodes, BranchFactor bf) {
  double ratio;
  switch (bf) {
    case BF2:
      ratio = 1.4;
      break;
    case BF4:
      ratio = 2.8;
      break;
    case BF8:
      ratio = 4;
      break;
    case BF32:
      ratio = 16;
      break;
  }
  uint32_t total = 0;
  while (num_leaf_nodes) {
    num_leaf_nodes = (uint32_t)(num_leaf_nodes / ratio);
    total += num_leaf_nodes;
  }
  return total;
}

/*
 * Look at the number of bytes needed to represent the leaf nodes, ignoring
 * both empty (not encoded) and filled (zero encoded at a higher layer) nodes.
 * Choose the BranchFactor that uses the least bytes.
 */
BranchFactor ChooseBranchFactor(const hb_set_t& set,
                                vector<uint32_t>& filled_twigs /* OUT */) {
  uint32_t empty_leaves[BF32 + 1]{};

  // "Twigs" are one level above leaves.
  // Zero-encoding happens at this level or above.
  // Only consider the twig level here.
  vector<uint32_t> all_filled_twigs[BF32 + 1];

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  if (!hb_set_next(&set, &cp)) {
    return BF8;
  }
  // 0 .. cp-1 are missing/empty (if any).
  AdvanceToCp(UINT32_MAX, cp, empty_leaves);
  uint32_t seq_len = 1;
  uint32_t prev_cp = cp;
  while (hb_set_next(&set, &cp)) {
    AdvanceToCp(prev_cp, cp, empty_leaves);
    if (cp == prev_cp + 1) {
      seq_len++;
    } else {
      seq_len = 1;
    }
    for (BranchFactor bf : {BF2, BF4, BF8, BF32}) {
      uint32_t twig_size = kBFTwigSize[bf];
      if ((cp + 1) % twig_size == 0) {
        if (seq_len >= twig_size) {
          all_filled_twigs[bf].push_back(cp / twig_size);
        }
      } else {
        break;
      }
    }
    prev_cp = cp;
  }

  uint32_t bytes[BF32 + 1];
  for (BranchFactor bf : {BF2, BF4, BF8, BF32}) {
    // We probably did not see the entire range encoded by the leaf layer of the
    // tree for this set (depth depends on BF and max value). The remaining
    // leaves will all be empty and can be ignored. Finish off current node /
    // round up to next node.
    uint32_t remainder = (prev_cp + 1) % kBFNodeSize[bf];
    if (remainder) {
      prev_cp += kBFNodeSize[bf] - remainder;
    }
    uint32_t processed_leaves = (prev_cp + 1) / kBFNodeSize[bf];
    // Of the leaves we processed, throw out the empty ones and the filled ones.
    // These are the nodes that will be encoded. Each twig represents multiple
    // leaves.
    uint32_t leaf_nodes = processed_leaves - empty_leaves[bf] -
                          (all_filled_twigs[bf].size() * kBFNodeSize[bf]);
    // Now estimate the size of the rest of the tree above the leaves.
    uint32_t tree_nodes = EstimateTreeSize(leaf_nodes, bf);
    // Compute size in bytes.
    switch (bf) {
      case BF2:
        bytes[bf] = (leaf_nodes + tree_nodes) / 4;
        break;
      case BF4:
        bytes[bf] = (leaf_nodes + tree_nodes) / 2;
        break;
      case BF8:
        bytes[bf] = (leaf_nodes + tree_nodes);
        break;
      case BF32:
        bytes[bf] = (leaf_nodes + tree_nodes) * 4;
        break;
    }
  }

  // Pick the one that saves the most bytes, defaulting to order BF4, BF2,
  // BF32, BF8 in the case of ties.
  BranchFactor optimal = BF4;
  for (BranchFactor bf : {BF2, BF32, BF8}) {
    if (bytes[bf] < bytes[optimal]) {
      optimal = bf;
    }
  }
  filled_twigs.swap(all_filled_twigs[optimal]);
  return optimal;
}

vector<uint32_t> FindFilledTwigs(const hb_set_t& set, BranchFactor bf) {
  uint32_t twig_size = kBFTwigSize[bf];
  uint32_t prev_cp = UINT32_MAX - 1;
  uint32_t seq_len = 0;
  vector<uint32_t> filled_twigs;
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID; hb_set_next(&set, &cp);) {
    if (cp == prev_cp + 1) {
      seq_len++;
    } else {
      seq_len = 1;
    }
    if ((cp + 1) % twig_size == 0) {
      if (seq_len == twig_size) {
        filled_twigs.push_back(cp / twig_size);
      }
      seq_len = 0;
    }
    prev_cp = cp;
  }
  return filled_twigs;
}

/*
 * Determines which nodes are completely filled, and thus should be encoded
 * with a zero. Leaf nodes are never marked as filled - writing all 0s instead
 * of all ones would not save any bytes. So the length of the array is the
 * number of nodes one level above the leaf level. For a given codepoint CP,
 * the value stored at index CP / (bits_per_node * bits_per_node). The value
 * will be the tree depth (0 for root) at which the node is first completely
 * filled, and thus should be encoded as a zero. The value will be greater
 * than the tree height when no filled nodes exist for these codepoints.
 */
map<uint32_t, uint8_t> FindFilledNodes(const hb_set_t& set,
                                       uint32_t bits_per_node,
                                       uint32_t tree_height,
                                       const vector<uint32_t>& filled_twigs) {
  map<uint32_t, uint8_t> filled_levels;
  if (tree_height < 2 || filled_twigs.empty()) {
    return filled_levels;
  }
  // "Twigs" are nodes one layer above the leaves. Layer tree_height - 2.
  for (uint32_t filled_twig : filled_twigs) {
    filled_levels[filled_twig] = tree_height - 2;
  }

  // Now work our way up the layers, "merging" filled nodes by decrementing
  // their filled-at number. Start processing at the layer above the twigs.
  uint32_t node_size = bits_per_node;  // Number to twigs to consider as a node.
  for (int layer = (int)tree_height - 3; layer >= 0; layer--) {
    uint8_t target_level = layer + 1;
    uint32_t prev_twig = UINT32_MAX - 1;
    uint32_t seq_len = 0;
    uint32_t num_merged_nodes = 0;
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
  const uint32_t layer;
  const uint32_t bits_per_node;
  const uint32_t tree_height;
  const uint32_t twig_size;
  const uint32_t values_per_bit;
  const uint32_t node_size;
  const map<uint32_t, uint8_t>& filled_levels;
  const vector<uint32_t>& node_bases;
  int next_node_base;
  uint32_t node_base;
  uint32_t node_max;
  uint32_t node_mask;
  uint32_t filled_max;
  vector<uint32_t>& next_node_bases; /* OUT */
  BitOutputBuffer& bit_buffer;       /* OUT */
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
    for (int layer = context.tree_height - 2; layer > filled_depth; layer--) {
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
  uint32_t bit_index = (cp - context.node_base) / context.values_per_bit;
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
    // Default case needed by Bazel.
    default:
      return ERROR;
  }
}

void EncodeLayer(const hb_set_t& set, uint32_t layer, uint32_t tree_height,
                 uint32_t bits_per_node,
                 const map<uint32_t, uint8_t>& filled_levels,
                 const vector<uint32_t>& node_bases,
                 vector<uint32_t>& next_node_bases, /* OUT */
                 BitOutputBuffer& bit_buffer /* OUT */) {
  uint32_t values_per_bit =
      ValuesPerBitForLayer(layer, tree_height, bits_per_node);
  uint32_t node_size = values_per_bit * bits_per_node;
  uint32_t twig_size = bits_per_node * bits_per_node;
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

/*
 * Encodes the set as a sparse bit set with the given branch factor.
 * The fully filled twigs lists the twigs (1 level above leaves) that are
 * completely filled. For example, with BF4, a 1 in filled_twigs means that
 * values 16..31 are all present in the set.
 */
string EncodeSet(const hb_set_t& set, BranchFactor branch_factor,
                 const vector<uint32_t>& filled_twigs) {
  if (!hb_set_get_population(&set)) {
    return "";
  }
  uint32_t tree_height = TreeDepthFor(set, kBFNodeSize[branch_factor]);
  // Determine which nodes are completely filled; encode them with zero.
  map<uint32_t, uint8_t> filled_levels = FindFilledNodes(
      set, kBFNodeSize[branch_factor], tree_height, filled_twigs);
  BitOutputBuffer bit_buffer(branch_factor, tree_height);

  // Starting values of the encoding ranges of the nodes queued to be encoded.
  // Queue up the root node.
  vector<uint32_t> node_bases(1, 0);
  vector<uint32_t> next_node_bases;
  for (uint32_t layer = 0; layer < tree_height; layer++) {
    EncodeLayer(set, layer, tree_height, kBFNodeSize[branch_factor],
                filled_levels, node_bases, next_node_bases, bit_buffer);
    if (next_node_bases.empty()) {
      break;  // Filled nodes mean nothing left to encode.
    }
    node_bases.swap(next_node_bases);
    next_node_bases.clear();
  }
  return bit_buffer.to_string();
}

string SparseBitSet::Encode(const hb_set_t& set, BranchFactor branch_factor) {
  if (!hb_set_get_population(&set)) {
    return "";
  }
  return EncodeSet(set, branch_factor, FindFilledTwigs(set, branch_factor));
}

string SparseBitSet::Encode(const hb_set_t& set) {
  if (!hb_set_get_population(&set)) {
    return "";
  }
  vector<uint32_t> filled_twigs;
  BranchFactor branch_factor = ChooseBranchFactor(set, filled_twigs);
  return EncodeSet(set, branch_factor, filled_twigs);
}

}  // namespace patch_subset
