#ifndef PATCH_SUBSET_HARFBUZZ_SUBSETTER_H_
#define PATCH_SUBSET_HARFBUZZ_SUBSETTER_H_

#include "absl/status/status.h"
#include "common/font_data.h"
#include "hb.h"
#include "patch_subset/subsetter.h"

namespace patch_subset {

// Computes a subset using harfbuzz hb-subset library.
class HarfbuzzSubsetter : public Subsetter {
 public:
  HarfbuzzSubsetter() {}

  absl::Status Subset(const common::FontData& font, const hb_set_t& codepoints,
                      const std::string& client_state_table,
                      common::FontData* subset /* OUT */) const override;

  void CodepointsInFont(const common::FontData& font,
                        hb_set_t* codepoints) const override;

 private:
  bool ShouldRetainGids(const common::FontData& font) const;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_HARFBUZZ_SUBSETTER_H_
