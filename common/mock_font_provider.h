#ifndef COMMON_MOCK_FONT_PROVIDER_H_
#define COMMON_MOCK_FONT_PROVIDER_H_

#include <string>

#include "common/font_provider.h"
#include "gtest/gtest.h"

namespace common {

// Provides fonts by loading them from a directory on the file system.
class MockFontProvider : public FontProvider {
 public:
  MOCK_METHOD(absl::Status, GetFont, (const std::string& id, FontData* out),
              (const override));
};

}  // namespace common

#endif  // COMMON_MOCK_FONT_PROVIDER_H_
