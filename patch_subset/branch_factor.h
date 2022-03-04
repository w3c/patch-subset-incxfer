#ifndef PATCH_SUBSET_BRANCH_FACTOR_H_
#define PATCH_SUBSET_BRANCH_FACTOR_H_

#include <stdint.h>

namespace patch_subset {

enum BranchFactor { BF2, BF4, BF8, BF32 };

// How many children of each node?
const uint32_t kBFNodeSize[]{2, 4, 8, 32};

// How many children of each node, but log based 2.
// For use when bit shifting to multiply or divide by node size.
const uint32_t kBFNodeSizeLog2[]{1, 2, 3, 5};

// How many values are covered by a "Twig" - 1 layer above a leaf node.
const uint32_t kBFTwigSize[]{4, 16, 64, 1024};

// Max depth of trees. Enough to encode the entire 32 bit range 0..0xFFFFFFFF.
const uint32_t kBFMaxDepth[]{32, 16, 11, 7};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BRANCH_FACTOR_H_
