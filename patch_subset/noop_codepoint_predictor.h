#ifndef PATCH_SUBSET_NOOP_CODEPOINT_PREDICTOR_H_
#define PATCH_SUBSET_NOOP_CODEPOINT_PREDICTOR_H_

#include "patch_subset/codepoint_predictor.h"

namespace patch_subset {

/*
 * This version of the codepoint predictor does nothing.
 */
class NoopCodepointPredictor : public CodepointPredictor {
 public:
  inline void Predict(const hb_set_t* font_codepoints,
                      const hb_set_t* have_codepoints,
                      const hb_set_t* requested_codepoints, unsigned count,
                      hb_set_t* predicted_codepoints /* OUT */) const override {
    // Do nothing.
  }
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_NOOP_CODEPOINT_PREDICTOR_H_
