#include "patch_subset/codepoint_mapping_checksum_impl.h"

#include "gtest/gtest.h"
#include "patch_subset/fast_hasher.h"

namespace patch_subset {

class CodepointMappingChecksumImplTest : public ::testing::Test {
 protected:
  CodepointMappingChecksumImplTest()
      : hasher_(new FastHasher()), codepoint_checksum_(hasher_.get()) {}

  std::unique_ptr<Hasher> hasher_;
  CodepointMappingChecksumImpl codepoint_checksum_;
};

TEST_F(CodepointMappingChecksumImplTest, ChecksumOnlyDeltas) {
  // The checksum should be stable across architectures and time, so test
  // against the exact checksum values, which should not change in the future.
  CodepointRemappingProto proto;
  CompressedListProto* codepoint_ordering = proto.mutable_codepoint_ordering();

  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 13697173163671258619u);

  codepoint_ordering->add_deltas(1);
  codepoint_ordering->add_deltas(5);
  codepoint_ordering->add_deltas(7);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 11859578335071439896u);

  // Adding a delta should change the checksum.
  codepoint_ordering->add_deltas(9);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 8783508627416541202u);

  // Check that the ordering of the deltas matters.
  codepoint_ordering->clear_deltas();
  codepoint_ordering->add_deltas(7);
  codepoint_ordering->add_deltas(5);
  codepoint_ordering->add_deltas(1);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 7767636834524416850u);
}

}  // namespace patch_subset
