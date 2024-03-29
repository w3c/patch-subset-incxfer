#include "patch_subset/cbor/axis_interval.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;

class AxisIntervalTest : public ::testing::Test {};

TEST_F(AxisIntervalTest, IsPoint) {
  AxisInterval interval;
  EXPECT_FALSE(interval.IsPoint());

  interval.SetStart(10.0f);
  EXPECT_TRUE(interval.IsPoint());

  interval.SetEnd(15.0f);
  EXPECT_FALSE(interval.IsPoint());

  interval.SetStart(15.0f);
  EXPECT_TRUE(interval.IsPoint());
}

TEST_F(AxisIntervalTest, Equal) {
  AxisInterval a;
  AxisInterval b;

  EXPECT_TRUE(a == b);

  a.SetStart(10.0f);
  EXPECT_FALSE(a == b);

  b.SetStart(10.0f);
  EXPECT_TRUE(a == b);

  a.SetEnd(10.0f);
  EXPECT_TRUE(a == b);

  a.SetEnd(15.0f);
  EXPECT_FALSE(a == b);

  b.SetEnd(15.0f);
  EXPECT_TRUE(a == b);
}

// TODO test end when not set.

TEST_F(AxisIntervalTest, IsValid) {
  AxisInterval interval;
  EXPECT_TRUE(interval.IsValid());

  interval.SetEnd(10.0f);
  EXPECT_FALSE(interval.IsValid());

  interval.SetStart(5.0f);
  EXPECT_TRUE(interval.IsValid());

  interval.SetEnd(2.5f);
  EXPECT_FALSE(interval.IsValid());

  interval.ResetEnd();
  EXPECT_TRUE(interval.IsValid());
}

TEST_F(AxisIntervalTest, Getters) {
  AxisInterval interval;
  EXPECT_FALSE(interval.HasStart());
  EXPECT_FALSE(interval.HasEnd());

  interval.SetStart(10.0f);
  EXPECT_TRUE(interval.HasStart());
  EXPECT_TRUE(interval.HasEnd());
  EXPECT_EQ(interval.Start(), 10.0f);
  EXPECT_EQ(interval.End(), 10.0f);

  interval.SetEnd(15.0f);
  EXPECT_TRUE(interval.HasStart());
  EXPECT_TRUE(interval.HasEnd());
  EXPECT_EQ(interval.End(), 15.0f);
}

TEST_F(AxisIntervalTest, EncodeInvalid) {
  AxisInterval interval;
  interval.SetStart(2.0f);
  interval.SetEnd(1.0f);

  cbor_item_unique_ptr map = empty_cbor_ptr();
  Status sc = interval.Encode(map);
  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(AxisIntervalTest, Encode) {
  AxisInterval interval;
  interval.SetStart(1.0f);
  interval.SetEnd(2.0f);

  cbor_item_unique_ptr map = empty_cbor_ptr();
  Status sc = interval.Encode(map);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(map.get(), nullptr);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  float value;
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  sc = CborUtils::DecodeFloat(*field, &value);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(value, 1.0f);

  field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 1, field);
  ASSERT_EQ(sc, absl::OkStatus());
  sc = CborUtils::DecodeFloat(*field, &value);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(value, 2.0f);
}

TEST_F(AxisIntervalTest, EncodePoint) {
  AxisInterval interval;
  interval.SetStart(10.0f);
  interval.SetEnd(10.0f);

  cbor_item_unique_ptr map = empty_cbor_ptr();
  Status sc = interval.Encode(map);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(map.get(), nullptr);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  float value;
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  sc = CborUtils::DecodeFloat(*field, &value);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(value, 10.0f);

  field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 1, field);
  ASSERT_TRUE(absl::IsNotFound(sc));
}

TEST_F(AxisIntervalTest, DecodeInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(2);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeFloat(10.0f))),
      absl::OkStatus());
  ASSERT_EQ(
      CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeFloat(5.0f))),
      absl::OkStatus());

  AxisInterval interval;
  Status sc = AxisInterval::Decode(*map, interval);
  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(AxisIntervalTest, Decode) {
  cbor_item_unique_ptr map = make_cbor_map(2);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeFloat(5.0f))),
      absl::OkStatus());
  ASSERT_EQ(
      CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeFloat(10.0f))),
      absl::OkStatus());

  AxisInterval interval;
  Status sc = AxisInterval::Decode(*map, interval);
  ASSERT_EQ(sc, absl::OkStatus());

  AxisInterval expected(5.0f, 10.f);
  ASSERT_EQ(interval, expected);
}

}  // namespace patch_subset::cbor
