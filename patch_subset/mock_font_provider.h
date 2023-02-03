#ifndef PATCH_SUBSET_MOCK_FONT_PROVIDER_H_
#define PATCH_SUBSET_MOCK_FONT_PROVIDER_H_

#include <string>

#include "gtest/gtest.h"
#include "patch_subset/font_provider.h"

namespace patch_subset {

// Provides fonts by loading them from a directory on the file system.
class MockFontProvider : public FontProvider {
 public:
  MOCK_METHOD(absl::Status, GetFont, (const std::string& id, FontData* out),
              (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_FONT_PROVIDER_H_
