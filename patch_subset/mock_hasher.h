#ifndef PATCH_SUBSET_MOCK_HASHER_H_
#define PATCH_SUBSET_MOCK_HASHER_H_

#include "absl/strings/string_view.h"
#include "common/hasher.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace patch_subset {

class MockHasher : public common::Hasher {
 public:
  MOCK_METHOD(uint64_t, Checksum, (::absl::string_view data), (const override));
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_HASHER_H_
