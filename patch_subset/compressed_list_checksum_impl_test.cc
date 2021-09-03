#include "patch_subset/compressed_list_checksum_impl.h"

#include "gtest/gtest.h"
#include "patch_subset/fast_hasher.h"

namespace patch_subset {

class CompressedListChecksumImplTest : public ::testing::Test {
 protected:
  CompressedListChecksumImplTest()
      : hasher_(new FastHasher()), compressed_list_checksum_(hasher_.get()) {}

  std::unique_ptr<Hasher> hasher_;
  CompressedListChecksumImpl compressed_list_checksum_;
};

TEST_F(CompressedListChecksumImplTest, ChecksumOnlyDeltas) {
  // The checksum should be stable across architectures and time, so test
  // against the exact checksum values, which should not change in the future.
  CompressedListProto codepoint_ordering;

  EXPECT_EQ(compressed_list_checksum_.Checksum(codepoint_ordering),
            18084600419918833904u);

  codepoint_ordering.add_deltas(1);
  codepoint_ordering.add_deltas(5);
  codepoint_ordering.add_deltas(7);
  EXPECT_EQ(compressed_list_checksum_.Checksum(codepoint_ordering),
            8699463713164181384u);

  // Adding a delta should change the checksum.
  codepoint_ordering.add_deltas(9);
  EXPECT_EQ(compressed_list_checksum_.Checksum(codepoint_ordering),
            17607134147851418686u);

  // Check that the ordering of the deltas matters.
  codepoint_ordering.clear_deltas();
  codepoint_ordering.add_deltas(7);
  codepoint_ordering.add_deltas(5);
  codepoint_ordering.add_deltas(1);
  EXPECT_EQ(compressed_list_checksum_.Checksum(codepoint_ordering),
            5871708787516736757u);
}

}  // namespace patch_subset
