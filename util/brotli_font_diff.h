#ifndef UTIL_BROTLI_FONT_DIFF_H_
#define UTIL_BROTLI_FONT_DIFF_H_

#include "common/status.h"
#include "hb-subset.h"
#include "patch_subset/font_data.h"

namespace util {

/*
 * Produces a brotli binary diff between two fonts. Uses knowledge of the underlying font format
 * to more efficiently produce a diff.
 */
class BrotliFontDiff {
 public:
  BrotliFontDiff() {}

  patch_subset::StatusCode Diff(hb_subset_plan_t* base_plan,
                                hb_face_t* base_face,
                                hb_subset_plan_t* derived_plan,
                                hb_face_t* derived_face,
                                patch_subset::FontData* patch) const;
};

}  // namespace util

#endif  // UTIL_BROTLI_FONT_DIFF_H_
