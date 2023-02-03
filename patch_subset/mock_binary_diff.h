#ifndef PATCH_SUBSET_MOCK_BINARY_DIFF_H_
#define PATCH_SUBSET_MOCK_BINARY_DIFF_H_

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/binary_diff.h"

namespace patch_subset {

class MockBinaryDiff : public BinaryDiff {
 public:
  MOCK_METHOD(absl::StatusCode, Diff,
              (const FontData& font_base, const FontData& font_derived,
               FontData* patch /* OUT */),
              (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_BINARY_DIFF_H_
