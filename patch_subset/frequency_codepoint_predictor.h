#ifndef PATCH_SUBSET_FREQUENCY_CODEPOINT_PREDICTOR_H_
#define PATCH_SUBSET_FREQUENCY_CODEPOINT_PREDICTOR_H_

#include <string>
#include <vector>

#include "analysis/pfe_methods/unicode_range_data/slicing_strategy.pb.h"
#include "codepoint_predictor.h"
#include "hb.h"

namespace patch_subset {

/*
 * Provides predictions on what additional codepoints a client might
 * need based on the codepoints requested and the set of codepoints
 * in the font being augmented.
 */
class FrequencyCodepointPredictor : public CodepointPredictor {
 public:
  /*
   * Create a new FrequencyCodepointPredictor instance. It will only predict
   * codepoints which are at least minimum_frequency (the ratio between the
   * codepoints count and the most frequent codepoints count).
   */
  static FrequencyCodepointPredictor* Create(float minimum_frequency);

  static FrequencyCodepointPredictor* Create(float minimum_frequency,
                                             const std::string& directory);

  void Predict(const hb_set_t* font_codepoints, const hb_set_t* have_codepoints,
               const hb_set_t* requested_codepoints, unsigned max,
               hb_set_t* predicted_codepoints /* OUT */) const;

 private:
  FrequencyCodepointPredictor(
      float minimum_frequency,
      std::vector<analysis::pfe_methods::unicode_range_data::SlicingStrategy>
          strategies);

  bool Intersects(
      const analysis::pfe_methods::unicode_range_data::Subset& subset,
      const hb_set_t* requested_codepoints) const;

  int IntersectionSize(
      const analysis::pfe_methods::unicode_range_data::SlicingStrategy& subset,
      const hb_set_t* requested_codepoints) const;

  const analysis::pfe_methods::unicode_range_data::SlicingStrategy*
  BestStrategyFor(const hb_set_t* codepoints) const;

  int HighestFrequencyCount(
      const analysis::pfe_methods::unicode_range_data::SlicingStrategy*
          strategy,
      const hb_set_t* font_codepoints,
      const hb_set_t* requested_codepoints) const;

  float minimum_frequency_;
  std::vector<analysis::pfe_methods::unicode_range_data::SlicingStrategy>
      strategies_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_FREQUENCY_CODEPOINT_PREDICTOR_H_
