#include "patch_subset/codepoint_mapping_checksum_impl.h"

#include "gtest/gtest.h"
#include "patch_subset/farm_hasher.h"

namespace patch_subset {

class CodepointMappingChecksumImplTest : public ::testing::Test {
 protected:
  CodepointMappingChecksumImplTest()
      : hasher_(new FarmHasher()), codepoint_checksum_(hasher_.get()) {}

  std::unique_ptr<Hasher> hasher_;
  CodepointMappingChecksumImpl codepoint_checksum_;
};

TEST_F(CodepointMappingChecksumImplTest, ChecksumOnlyDeltas) {
  // The checksum should be stable across architectures and time, so test
  // against the exact checksum values, which should not change in the future.
  CodepointRemappingProto proto;
  CompressedListProto* codepoint_ordering = proto.mutable_codepoint_ordering();

  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 10786723806386174706u);

  codepoint_ordering->add_deltas(1);
  codepoint_ordering->add_deltas(5);
  codepoint_ordering->add_deltas(7);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 3761381749415832926u);

  // Adding a delta should change the checksum.
  codepoint_ordering->add_deltas(9);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 17013761565217376327u);

  // Check that the ordering of the deltas matters.
  codepoint_ordering->clear_deltas();
  codepoint_ordering->add_deltas(7);
  codepoint_ordering->add_deltas(5);
  codepoint_ordering->add_deltas(1);
  EXPECT_EQ(codepoint_checksum_.Checksum(proto), 3695224139272583709u);
}

}  // namespace patch_subset
