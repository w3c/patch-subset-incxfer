#include "patch_subset/vcdiff_binary_diff.h"

#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "common/font_data.h"
#include "google/vcencoder.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using common::FontData;

Status VCDIFFBinaryDiff::Diff(const FontData& font_base,
                              const FontData& font_derived,
                              FontData* patch /* OUT */) const {
  open_vcdiff::VCDiffEncoder encoder(font_base.data(), font_base.size());
  encoder.SetFormatFlags(open_vcdiff::VCD_STANDARD_FORMAT);
  std::string diff;
  if (!encoder.Encode(font_derived.data(), font_derived.size(), &diff)) {
    return absl::InternalError("VCDIFF encoding failed.");
  }

  patch->copy(diff.c_str(), diff.size());

  return absl::OkStatus();
}

}  // namespace patch_subset
