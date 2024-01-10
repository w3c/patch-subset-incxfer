#include "patch_subset/integer_list_checksum_impl.h"

#include "common/fast_hasher.h"
#include "common/hasher.h"
#include "gtest/gtest.h"

namespace patch_subset {

using common::FastHasher;
using common::Hasher;
using std::unique_ptr;
using std::vector;

class IntegerListChecksumImplTest : public ::testing::Test {
 protected:
  IntegerListChecksumImplTest()
      : hasher_(new FastHasher()), integer_list_checksum_(hasher_.get()) {}

  unique_ptr<Hasher> hasher_;
  IntegerListChecksumImpl integer_list_checksum_;
};

TEST_F(IntegerListChecksumImplTest, ChecksumOnlyDeltas) {
  // The checksum should be stable across architectures and time, so test
  // against the exact checksum values, which should not change in the future.
  vector<int32_t> codepoint_ordering;

  EXPECT_EQ(integer_list_checksum_.Checksum(codepoint_ordering),
            18084600419918833904u);

  codepoint_ordering.push_back(1);
  codepoint_ordering.push_back(6);
  codepoint_ordering.push_back(13);
  EXPECT_EQ(integer_list_checksum_.Checksum(codepoint_ordering),
            8699463713164181384u);

  // Adding an int should change the checksum.
  codepoint_ordering.push_back(22);
  EXPECT_EQ(integer_list_checksum_.Checksum(codepoint_ordering),
            17607134147851418686u);

  // Check that the ordering of the ints matters.
  codepoint_ordering.clear();
  codepoint_ordering.push_back(7);
  codepoint_ordering.push_back(12);
  codepoint_ordering.push_back(13);
  EXPECT_EQ(integer_list_checksum_.Checksum(codepoint_ordering),
            5871708787516736757u);
}

TEST_F(IntegerListChecksumImplTest, SpecChecksumExample) {
  // Test that the checksum example given in the spec gets the same result:
  // https://w3c.github.io/IFT/Overview.html#reordering-checksum
  // Checksum of [106, 97, 105, 120, 100] == 0x6986dc19f4e621e
  vector<int32_t> codepoint_ordering;

  codepoint_ordering.push_back(106);
  codepoint_ordering.push_back(97);
  codepoint_ordering.push_back(105);
  codepoint_ordering.push_back(120);
  codepoint_ordering.push_back(100);
  EXPECT_EQ(integer_list_checksum_.Checksum(codepoint_ordering),
            0x6986dc19f4e621eu);
}

}  // namespace patch_subset
