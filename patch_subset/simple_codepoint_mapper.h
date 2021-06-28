#ifndef PATCH_SUBSET_SIMPLE_CODEPOINT_MAPPER_H_
#define PATCH_SUBSET_SIMPLE_CODEPOINT_MAPPER_H_

#include <vector>

#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/codepoint_mapper.h"

namespace patch_subset {

// Computes a simple mapping where the codepoints
// are sorted by value and then mapped to their
// index in that sorting.
class SimpleCodepointMapper : public CodepointMapper {
 public:
  SimpleCodepointMapper() {}

  void ComputeMapping(const hb_set_t* codepoints,
                      CodepointMap* mapping) const override;

 private:
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_SIMPLE_CODEPOINT_MAPPER_H_
