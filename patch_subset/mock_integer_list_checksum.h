#ifndef PATCH_SUBSET_MOCK_INTEGER_LIST_CHECKSUM_H_
#define PATCH_SUBSET_MOCK_INTEGER_LIST_CHECKSUM_H_

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/integer_list_checksum.h"

namespace patch_subset {

class MockIntegerListChecksum : public IntegerListChecksum {
 public:
  MOCK_METHOD(uint64_t, Checksum, (const std::vector<int32_t>& ints),
              (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_INTEGER_LIST_CHECKSUM_H_
