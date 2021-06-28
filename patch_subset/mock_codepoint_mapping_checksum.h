#ifndef PATCH_SUBSET_MOCK_CODEPOINT_MAPPING_CHECKSUM_H_
#define PATCH_SUBSET_MOCK_CODEPOINT_MAPPING_CHECKSUM_H_

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/codepoint_mapping_checksum.h"

namespace patch_subset {

class MockCodepointMappingChecksum : public CodepointMappingChecksum {
 public:
  MOCK_METHOD(uint64_t, Checksum, (const CodepointRemappingProto& proto),
              (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_CODEPOINT_MAPPING_CHECKSUM_H_
