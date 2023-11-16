#ifndef COMMON_FONT_PROVIDER_H_
#define COMMON_FONT_PROVIDER_H_

#include <string>

#include "absl/status/status.h"
#include "common/font_data.h"

namespace common {

// Interface for an object which can provide font binaries associated
// with a key.
class FontProvider {
 public:
  virtual ~FontProvider() = default;

  // Load fontdata associated with it and write it into out.
  // Returns false if the id was not recognized and the font
  // failed to load.
  virtual absl::Status GetFont(const std::string& id, FontData* out) const = 0;
};

}  // namespace common

#endif  // COMMON_FONT_PROVIDER_H_
