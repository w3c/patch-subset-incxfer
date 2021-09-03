#ifndef PATCH_SUBSET_MOCK_COMPRESSED_LIST_CHECKSUM_H_
#define PATCH_SUBSET_MOCK_COMPRESSED_LIST_CHECKSUM_H_

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/compressed_list_checksum.h"

namespace patch_subset {

class MockCompressedListChecksum : public CompressedListChecksum {
 public:
  MOCK_METHOD(uint64_t, Checksum, (const CompressedListProto& proto),
              (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_COMPRESSED_LIST_CHECKSUM_H_
