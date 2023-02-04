#include "patch_subset/cbor/range_list.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::Status;
using std::optional;
using std::vector;

class RangeListTest : public ::testing::Test {};

TEST_F(RangeListTest, Encode) {
  vector<range> input{{1, 10}, {20, 30}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  Status sc = RangeList::Encode(input, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(result.get(), nullptr);
  vector<int32_t> ints;
  sc = IntegerList::DecodeSorted(*result, ints);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<int32_t> expected{1, 10, 20, 30};
  ASSERT_EQ(ints, expected);
}

TEST_F(RangeListTest, EncodeUnsorted1) {
  vector<range> input{{10, 20}, {1, 10}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  Status sc = RangeList::Encode(input, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(RangeListTest, EncodeUnsorted2) {
  vector<range> input{{20, 10}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  Status sc = RangeList::Encode(input, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(RangeListTest, Decode) {
  vector<int32_t> ints{5, 10, 15, 20};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  Status sc = IntegerList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<range> ranges;

  sc = RangeList::Decode(*input, ranges);

  ASSERT_EQ(sc, absl::OkStatus());
  vector<range> expected{{5, 10}, {15, 20}};
  ASSERT_EQ(ranges, expected);
}

TEST_F(RangeListTest, DecodeEmpty) {
  vector<int32_t> ints{};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  Status sc = IntegerList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<range> ranges;

  sc = RangeList::Decode(*input, ranges);

  ASSERT_EQ(sc, absl::OkStatus());
  vector<range> expected{};
  ASSERT_EQ(ranges, expected);
}

TEST_F(RangeListTest, DecodeOddNumberOfValues) {
  vector<int32_t> ints{1, 10, 100};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  Status sc = IntegerList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<range> ranges;

  sc = RangeList::Decode(*input, ranges);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_TRUE(ranges.empty());
}

TEST_F(RangeListTest, SetRangeListFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<range_vector> r({{1, 10}});

  Status sc = RangeList::SetRangeListField(*map, 0, r);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  range_vector result;
  sc = RangeList::Decode(*field, result);
  ASSERT_EQ(sc, absl::OkStatus());
  optional<range_vector> expected({{1, 10}});
  ASSERT_EQ(result, expected);
}

TEST_F(RangeListTest, SetRangeListFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<range_vector> r;

  Status sc = RangeList::SetRangeListField(*map, 0, r);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(RangeListTest, GetRangeListField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr field = empty_cbor_ptr();
  ASSERT_EQ(IntegerList::EncodeSorted({1, 100}, field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, 0, move_out(field)), absl::OkStatus());
  optional<range_vector> result;

  Status sc = RangeList::GetRangeListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  range_vector expected({{1, 100}});
  ASSERT_EQ(result, expected);
}

TEST_F(RangeListTest, GetRangeListFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<range_vector> result({{1, 100}});

  Status sc = RangeList::GetRangeListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(RangeListTest, GetRangeListFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeInt(0))),
            absl::OkStatus());
  optional<range_vector> result({{1, 100}});

  Status sc = RangeList::GetRangeListField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  range_vector expected({{1, 100}});
  ASSERT_EQ(result, expected);
}

}  // namespace patch_subset::cbor
