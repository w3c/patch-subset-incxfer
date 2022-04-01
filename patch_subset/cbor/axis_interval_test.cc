#include "patch_subset/cbor/axis_interval.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

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
  EXPECT_FALSE(interval.HasEnd());
  EXPECT_EQ(interval.Start(), 10.0f);


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
  StatusCode sc = interval.Encode(map);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(AxisIntervalTest, Encode) {
  AxisInterval interval;
  interval.SetStart(1.0f);
  interval.SetEnd(2.0f);

  cbor_item_unique_ptr map = empty_cbor_ptr();
  StatusCode sc = interval.Encode(map);


  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(map.get(), nullptr);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  float value;
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeFloat(*field, &value);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(value, 1.0f);

  field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeFloat(*field, &value);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(value, 2.0f);
}

TEST_F(AxisIntervalTest, DecodeInvalid) {

  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeFloat(10.0f)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeFloat(5.0f)));

  AxisInterval interval;
  StatusCode sc = AxisInterval::Decode(*map, interval);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(AxisIntervalTest, Decode) {

  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeFloat(5.0f)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeFloat(10.0f)));

  AxisInterval interval;
  StatusCode sc = AxisInterval::Decode(*map, interval);
  ASSERT_EQ(sc, StatusCode::kOk);

  AxisInterval expected(5.0f, 10.f);
  ASSERT_EQ(interval, expected);
}

}  // namespace patch_subset::cbor
