#include "patch_subset/vcdiff_binary_diff.h"

#include <vector>

#include "common/logging.h"
#include "common/status.h"
#include "google/vcencoder.h"
#include "patch_subset/font_data.h"

using ::absl::string_view;

namespace patch_subset {

StatusCode VCDIFFBinaryDiff::Diff(const FontData& font_base,
                                  const FontData& font_derived,
                                  FontData* patch /* OUT */) const {
  open_vcdiff::VCDiffEncoder encoder(font_base.data(), font_base.size());
  encoder.SetFormatFlags(open_vcdiff::VCD_STANDARD_FORMAT);
  std::string diff;
  if (!encoder.Encode(font_derived.data(), font_derived.size(), &diff))
    return StatusCode::kInternal;

  patch->copy(diff.c_str(), diff.size());

  return StatusCode::kOk;
}

}  // namespace patch_subset
