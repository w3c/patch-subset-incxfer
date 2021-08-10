#include "patch_subset/harfbuzz_subsetter.h"

#include "common/logging.h"
#include "common/status.h"
#include "hb-subset.h"
#include "hb_set_unique_ptr.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

static unsigned int kRetainGidsThreshold = 5000;

bool HarfbuzzSubsetter::ShouldRetainGids(const FontData& font) const {
  hb_set_unique_ptr codepoints = make_hb_set();
  CodepointsInFont(font, codepoints.get());
  return hb_set_get_population(codepoints.get()) < kRetainGidsThreshold;
}

StatusCode HarfbuzzSubsetter::Subset(const FontData& font,
                                     const hb_set_t& codepoints,
                                     FontData* subset /* OUT */) const {
  if (!hb_set_get_population(&codepoints)) {
    subset->reset();
    return StatusCode::kOk;
  }

  hb_blob_t* blob = font.reference_blob();
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  hb_subset_input_t* input = hb_subset_input_create_or_fail();
  hb_set_t* input_codepoints = hb_subset_input_unicode_set(input);
  hb_set_union(input_codepoints, &codepoints);

  // For subsetting we want normally to retain glyph ids, since this helps
  // reduce patch size by keeping the ids consistent between patches. However
  // for fonts with large numbers of codepoints the additional overhead added
  // encoding the empty gids is not worth the future savings on patch size.
  hb_subset_input_set_flags(input, ShouldRetainGids(font)
                                       ? HB_SUBSET_FLAGS_RETAIN_GIDS
                                       : HB_SUBSET_FLAGS_DEFAULT);

  hb_face_t* subset_face = hb_subset_or_fail(face, input);
  hb_subset_input_destroy(input);
  hb_face_destroy(face);

  if (!subset_face) {
    hb_face_destroy(subset_face);
    LOG(WARNING) << "Internal subsetting failure.";
    return StatusCode::kInternal;
  }

  hb_blob_t* subset_blob = hb_face_reference_blob(subset_face);
  hb_face_destroy(subset_face);

  subset->set(subset_blob);

  hb_blob_destroy(subset_blob);

  return StatusCode::kOk;
}

void HarfbuzzSubsetter::CodepointsInFont(const FontData& font,
                                         hb_set_t* codepoints) const {
  hb_blob_t* blob = font.reference_blob();
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  hb_face_collect_unicodes(face, codepoints);
  hb_face_destroy(face);
}

}  // namespace patch_subset
