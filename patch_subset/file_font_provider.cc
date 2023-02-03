#include "patch_subset/file_font_provider.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

using absl::StatusCode;

StatusCode FileFontProvider::GetFont(const std::string& id,
                                     FontData* out) const {
  std::string path = base_directory_ + id;
  hb_blob_t* blob = hb_blob_create_from_file(path.c_str());
  if (!hb_blob_get_length(blob)) {
    hb_blob_destroy(blob);
    LOG(WARNING) << path << " does not exist.";
    return StatusCode::kNotFound;
  }

  out->set(blob);
  hb_blob_destroy(blob);

  return StatusCode::kOk;
}

}  // namespace patch_subset
