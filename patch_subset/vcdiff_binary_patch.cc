#include "patch_subset/vcdiff_binary_patch.h"

#include <vector>

#include "common/logging.h"
#include "common/status.h"
#include "google/vcdecoder.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

using ::absl::string_view;

namespace patch_subset {

StatusCode VCDIFFBinaryPatch::Patch(const FontData& font_base,
                                    const FontData& patch,
                                    FontData* font_derived /* OUT */) const {
  open_vcdiff::VCDiffDecoder decoder;
  std::string result;
  if (!decoder.Decode(font_base.data(), font_base.size(), patch.string(),
                      &result))
    return StatusCode::kInvalidArgument;
  font_derived->copy(result.c_str(), result.size());
  return StatusCode::kOk;
}

}  // namespace patch_subset
