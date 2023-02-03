#include "patch_subset/vcdiff_binary_patch.h"

#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "google/vcdecoder.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;

Status VCDIFFBinaryPatch::Patch(const FontData& font_base,
                                const FontData& patch,
                                FontData* font_derived /* OUT */) const {
  open_vcdiff::VCDiffDecoder decoder;
  std::string result;
  if (!decoder.Decode(font_base.data(), font_base.size(), patch.string(),
                      &result)) {
    return absl::InvalidArgumentError("Unable to decode vcdiff patch.");
  }
  font_derived->copy(result.c_str(), result.size());
  return absl::OkStatus();
}

}  // namespace patch_subset
