#ifndef PATCH_SUBSET_MOCK_BINARY_PATCH_H_
#define PATCH_SUBSET_MOCK_BINARY_PATCH_H_

#include <string>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/binary_patch.h"

namespace patch_subset {

class MockBinaryPatch : public BinaryPatch {
 public:
  MOCK_METHOD(absl::Status, Patch,
              (const FontData& font_base, const FontData& patch,
               FontData* derived /* OUT */),
              (const override));
};

class ApplyPatch {
 public:
  explicit ApplyPatch(::absl::string_view patched) : patched_(patched) {}

  absl::Status operator()(const FontData& font_base, const FontData& patch,
                          FontData* font_derived) {
    font_derived->copy(patched_);
    return absl::OkStatus();
  }

 private:
  ::absl::string_view patched_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_BINARY_PATCH_H_
