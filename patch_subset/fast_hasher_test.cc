#include "patch_subset/fast_hasher.h"

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace patch_subset {

class FastHasherTest : public ::testing::Test {
 protected:
  FastHasherTest() : hasher_(new FastHasher()) {}

  std::unique_ptr<Hasher> hasher_;
};

TEST_F(FastHasherTest, SpecChecksumExample) {
  // Test that the checksum example given in the spec gets the same result:
  // https://w3c.github.io/IFT/Overview.html#computing-checksums
  //
  // hash(0f 7b 5a e5) == 0xe5e0d1dc89eaa189
  const uint8_t example_1[] = {0x0f, 0x7b, 0x5a, 0xe5};
  EXPECT_EQ(hasher_->Checksum(
                absl::string_view((const char*)example_1, sizeof(example_1))),
            0xe5e0d1dc89eaa189u);

  // hash(1d f4 02 5e d3 b8 43 21 3b ae de) == 0xb31e9c70768205fb
  const uint8_t example_2[] = {0x1d, 0xf4, 0x02, 0x5e, 0xd3, 0xb8,
                               0x43, 0x21, 0x3b, 0xae, 0xde};
  EXPECT_EQ(hasher_->Checksum(
                absl::string_view((const char*)example_2, sizeof(example_2))),
            0xb31e9c70768205fbu);
}

}  // namespace patch_subset
