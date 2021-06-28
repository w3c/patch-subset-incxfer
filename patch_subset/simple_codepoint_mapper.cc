#include "patch_subset/simple_codepoint_mapper.h"

#include "hb.h"

namespace patch_subset {

void SimpleCodepointMapper::ComputeMapping(const hb_set_t* codepoints,
                                           CodepointMap* mapping) const {
  mapping->Clear();
  int new_index = 0;
  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID;
       hb_set_next(codepoints, &cp);) {
    // hb_set_t iterates in sorted order by default. So just push the codepoints
    // in the set into the mapping in the default iteration order.
    mapping->AddMapping(cp, new_index++);
  }
}

}  // namespace patch_subset
