#include "patch_subset/cbor/axis_space.h"

#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/cbor/axis_interval.h"
#include "patch_subset/cbor/axis_space.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;

class AxisSpaceTest : public ::testing::Test {
 public:
  AxisSpaceTest()
      : space(),
        tag_a(HB_TAG('a', 'a', 'a', 'a')),
        tag_b(HB_TAG('b', 'b', 'b', 'b')),
        tag_c(HB_TAG('c', 'c', 'c', 'c')) {
    space.AddInterval(tag_a, AxisInterval(10));
    space.AddInterval(tag_c, AxisInterval(20));
    space.AddInterval(tag_c, AxisInterval(30, 40));
  }

  AxisSpace space;
  hb_tag_t tag_a;
  hb_tag_t tag_b;
  hb_tag_t tag_c;
};

TEST_F(AxisSpaceTest, Has) {
  EXPECT_TRUE(space.Has(tag_a));
  EXPECT_FALSE(space.Has(tag_b));
  EXPECT_TRUE(space.Has(tag_c));
}

TEST_F(AxisSpaceTest, Clear) {
  space.Clear(tag_a);
  EXPECT_FALSE(space.Has(tag_a));
  EXPECT_FALSE(space.Has(tag_b));
  EXPECT_TRUE(space.Has(tag_c));
}

TEST_F(AxisSpaceTest, IntervalsFor) {
  std::vector<AxisInterval> a{AxisInterval(10)};
  std::vector<AxisInterval> b;
  std::vector<AxisInterval> c{AxisInterval(20), AxisInterval(30, 40)};

  EXPECT_EQ(a, space.IntervalsFor(tag_a));
  EXPECT_EQ(b, space.IntervalsFor(tag_b));
  EXPECT_EQ(c, space.IntervalsFor(tag_c));
}

TEST_F(AxisSpaceTest, Equal) {
  AxisSpace space_copy(space);

  EXPECT_TRUE(space == space_copy);

  space_copy.AddInterval(tag_a, AxisInterval(30, 30));
  EXPECT_FALSE(space == space_copy);
}

TEST_F(AxisSpaceTest, EncodeDecode) {
  cbor_item_unique_ptr map = empty_cbor_ptr();
  ASSERT_EQ(space.Encode(map), absl::OkStatus());

  AxisSpace decoded;
  ASSERT_EQ(AxisSpace::Decode(*map, decoded), absl::OkStatus());
  EXPECT_EQ(space, decoded);
}

}  // namespace patch_subset::cbor
