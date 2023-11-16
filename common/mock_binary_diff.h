#ifndef COMMON_MOCK_BINARY_DIFF_H_
#define COMMON_MOCK_BINARY_DIFF_H_

#include <string>

#include "common/binary_diff.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace common {

class MockBinaryDiff : public BinaryDiff {
 public:
  MOCK_METHOD(absl::Status, Diff,
              (const FontData& font_base, const FontData& font_derived,
               FontData* patch /* OUT */),
              (const override));
};

}  // namespace common

#endif  // COMMON_MOCK_BINARY_DIFF_H_
