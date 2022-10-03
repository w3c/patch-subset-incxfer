#include "util/brotli_font_diff.h"

#include "absl/types/span.h"
#include "util/brotli_stream.h"

using ::absl::Span;
using ::patch_subset::StatusCode;
using ::patch_subset::FontData;

namespace util {

StatusCode BrotliFontDiff::Diff(hb_subset_plan_t* base_plan,
                                hb_face_t* base_face,
                                hb_subset_plan_t* derived_plan,
                                hb_face_t* derived_face,
                                FontData* patch) const
{
  hb_blob_t* base = hb_face_reference_blob(base_face);
  hb_blob_t* derived = hb_face_reference_blob(derived_face);

  BrotliStream out(22);

  // TODO(garretrieger): actually diff
  out.insert_uncompressed(Span<const uint8_t>((const uint8_t*) hb_blob_get_data(derived, nullptr),
                                              hb_blob_get_length(derived)));
  out.end_stream();

  patch->copy((const char*) out.compressed_data().data(),
              out.compressed_data().size());

  hb_blob_destroy(derived);
  hb_blob_destroy(base);

  return StatusCode::kOk;
}

}  // namespace util
