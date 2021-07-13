#include "patch_subset/cbor/compressed_range_list.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/compressed_int_list.h"

namespace patch_subset::cbor {

using std::optional;
using std::vector;

class CompressedRangeListTest : public ::testing::Test {};

TEST_F(CompressedRangeListTest, Encode) {
  vector<range> input{{1, 10}, {20, 30}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = CompressedRangeList::Encode(input, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result.get(), nullptr);
  vector<int32_t> ints;
  sc = CompressedIntList::DecodeSorted(*result, ints);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<int32_t> expected{1, 10, 20, 30};
  ASSERT_EQ(ints, expected);
}

TEST_F(CompressedRangeListTest, EncodeUnsorted1) {
  vector<range> input{{10, 20}, {1, 10}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = CompressedRangeList::Encode(input, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(CompressedRangeListTest, EncodeUnsorted2) {
  vector<range> input{{20, 10}};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = CompressedRangeList::Encode(input, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(CompressedRangeListTest, Decode) {
  vector<int32_t> ints{5, 10, 15, 20};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  StatusCode sc = CompressedIntList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<range> ranges;

  sc = CompressedRangeList::Decode(*input, ranges);

  ASSERT_EQ(sc, StatusCode::kOk);
  vector<range> expected{{5, 10}, {15, 20}};
  ASSERT_EQ(ranges, expected);
}

TEST_F(CompressedRangeListTest, DecodeEmpty) {
  vector<int32_t> ints{};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  StatusCode sc = CompressedIntList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<range> ranges;

  sc = CompressedRangeList::Decode(*input, ranges);

  ASSERT_EQ(sc, StatusCode::kOk);
  vector<range> expected{};
  ASSERT_EQ(ranges, expected);
}

TEST_F(CompressedRangeListTest, DecodeOddNumberOfValues) {
  vector<int32_t> ints{1, 10, 100};
  cbor_item_unique_ptr input = empty_cbor_ptr();
  StatusCode sc = CompressedIntList::EncodeSorted(ints, input);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<range> ranges;

  sc = CompressedRangeList::Decode(*input, ranges);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  ASSERT_TRUE(ranges.empty());
}

TEST_F(CompressedRangeListTest, SetRangeListFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<range_vector> r({{1, 10}});

  StatusCode sc = CompressedRangeList::SetRangeListField(*map, 0, r);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  range_vector result;
  sc = CompressedRangeList::Decode(*field, result);
  ASSERT_EQ(sc, StatusCode::kOk);
  optional<range_vector> expected({{1, 10}});
  ASSERT_EQ(result, expected);
}

TEST_F(CompressedRangeListTest, SetRangeListFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<range_vector> r;

  StatusCode sc = CompressedRangeList::SetRangeListField(*map, 0, r);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CompressedRangeListTest, GetRangeListField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr field = empty_cbor_ptr();
  CompressedIntList::EncodeSorted({1, 100}, field);
  CborUtils::SetField(*map, 0, move_out(field));
  optional<range_vector> result;

  StatusCode sc = CompressedRangeList::GetRangeListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  range_vector expected({{1, 100}});
  ASSERT_EQ(result, expected);
}

TEST_F(CompressedRangeListTest, GetRangeListFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<range_vector> result({{1, 100}});

  StatusCode sc = CompressedRangeList::GetRangeListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(result.has_value());
}

TEST_F(CompressedRangeListTest, GetRangeListFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeInt(0)));
  optional<range_vector> result({{1, 100}});

  StatusCode sc = CompressedRangeList::GetRangeListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  range_vector expected({{1, 100}});
  ASSERT_EQ(result, expected);
}

}  // namespace patch_subset::cbor
