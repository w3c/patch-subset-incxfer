#ifndef PATCH_SUBSET_FILE_FONT_PROVIDER_H_
#define PATCH_SUBSET_FILE_FONT_PROVIDER_H_

#include <string>

#include "patch_subset/font_provider.h"

namespace patch_subset {

// Provides fonts by loading them from a directory on the file system.
class FileFontProvider : public FontProvider {
 public:
  explicit FileFontProvider(const std::string& base_directory)
      : base_directory_(base_directory) {}

  absl::StatusCode GetFont(const std::string& id, FontData* out) const override;

 private:
  std::string base_directory_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_FILE_FONT_PROVIDER_H_
