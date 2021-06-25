#ifndef PATCH_SUBSET_CODEPOINT_PREDICTOR_H_
#define PATCH_SUBSET_CODEPOINT_PREDICTOR_H_

#include "hb.h"

namespace patch_subset {

/*
 * Provides predictions on what additional codepoints a client might
 * need based on the codepoints requested and the set of codepoints
 * in the font being augmented.
 */
class CodepointPredictor {
 public:
  virtual ~CodepointPredictor() = default;

  virtual void Predict(const hb_set_t* font_codepoints,
                       const hb_set_t* have_codepoints,
                       const hb_set_t* requested_codepoints, unsigned max,
                       hb_set_t* predicted_codepoints /* OUT */) const = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_CODEPOINT_PREDICTOR_H_
