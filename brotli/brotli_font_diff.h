#ifndef BROTLI_BROTLI_FONT_DIFF_H_
#define BROTLI_BROTLI_FONT_DIFF_H_

#include "absl/status/status.h"
#include "common/font_data.h"
#include "common/hb_set_unique_ptr.h"
#include "hb-subset.h"

namespace brotli {

/*
 * Produces a brotli binary diff between two fonts. Uses knowledge of the
 * underlying font format to more efficiently produce a diff.
 */
class BrotliFontDiff {
 public:
  // Sorts the tables in face_builder into the order expected by the font
  // differ.
  static void SortForDiff(const hb_set_t* immutable_tables,
                          const hb_set_t* custom_diff_tables,
                          const hb_face_t* original_face,
                          hb_face_t* face_builder /* IN/OUT */);

  BrotliFontDiff(const hb_set_t* immutable_tables,
                 const hb_set_t* custom_diff_tables)
      : immutable_tables_(hb_set_copy(immutable_tables), &hb_set_destroy),
        custom_diff_tables_(hb_set_copy(custom_diff_tables), &hb_set_destroy) {}

  absl::Status Diff(hb_subset_plan_t* base_plan, hb_blob_t* base,
                    hb_subset_plan_t* derived_plan, hb_blob_t* derived,
                    common::FontData* patch) const;

 private:
  common::hb_set_unique_ptr immutable_tables_;
  common::hb_set_unique_ptr custom_diff_tables_;
};

}  // namespace brotli

#endif  // BROTLI_BROTLI_FONT_DIFF_H_
