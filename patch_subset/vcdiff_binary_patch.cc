#include "patch_subset/vcdiff_binary_patch.h"

#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "common/font_data.h"
#include "google/vcdecoder.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using common::FontData;

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

Status VCDIFFBinaryPatch::Patch(const FontData& font_base,
                                const std::vector<FontData>& patch,
                                FontData* font_derived) const {
  if (patch.size() == 1) {
    return Patch(font_base, patch[0], font_derived);
  }

  if (patch.size() == 0) {
    return absl::InvalidArgumentError("Must provide at least one patch.");
  }

  return absl::InvalidArgumentError(
      "VCDIFF binary patches cannot be applied independently");
}

}  // namespace patch_subset
