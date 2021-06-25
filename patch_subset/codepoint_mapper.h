#ifndef PATCH_SUBSET_CODEPOINT_MAPPER_H_
#define PATCH_SUBSET_CODEPOINT_MAPPER_H_

#include <vector>

#include "hb.h"
#include "patch_subset/codepoint_map.h"

namespace patch_subset {

// Interface to a codepoint mapper.
class CodepointMapper {
 public:
  virtual ~CodepointMapper() = default;

  // Computes a mapping for the provided codepoints. The mapping
  // is written into the mapping vector.
  //
  // To interpret the mapping vector: codepoint value 'mapping[i]'
  // is mapped to value 'i'.
  virtual void ComputeMapping(const hb_set_t* codepoints,
                              CodepointMap* mapping) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_CODEPOINT_MAPPER_H_
