#include "patch_subset/cbor/compressed_set.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::string_view;
using std::string;

class CompressedSetTest : public ::testing::Test {};

TEST_F(CompressedSetTest, EmptyConstructor) {
  CompressedSet compressed_set;

  EXPECT_FALSE(compressed_set.HasSparseBitSetBytes());
  EXPECT_EQ(compressed_set.SparseBitSetBytes(), "");
  EXPECT_FALSE(compressed_set.HasRanges());
  EXPECT_TRUE(compressed_set.Ranges().empty());
}

TEST_F(CompressedSetTest, Constructor) {
  string bytes("010001010");
  range_vector ranges{{2, 4}, {4, 8}};

  CompressedSet compressed_set(bytes, ranges);

  EXPECT_TRUE(compressed_set.HasSparseBitSetBytes());
  EXPECT_EQ(compressed_set.SparseBitSetBytes(), bytes);
  EXPECT_TRUE(compressed_set.HasRanges());
  EXPECT_EQ(compressed_set.Ranges(), ranges);

  EXPECT_EQ(CompressedSet(compressed_set), compressed_set);
}

TEST_F(CompressedSetTest, CopyConstructor) {
  string bytes("010001010");
  range_vector ranges{{2, 4}, {4, 8}};
  CompressedSet compressed_set(bytes, ranges);

  EXPECT_EQ(CompressedSet(compressed_set), compressed_set);
}

TEST_F(CompressedSetTest, MoveConstructor) {
  string bytes("010001010");
  range_vector ranges{{2, 4}, {4, 8}};
  CompressedSet origional(bytes, ranges);
  CompressedSet origionalCopy(origional);

  CompressedSet moved = std::move(origional);

  EXPECT_EQ(moved, origionalCopy);
}

TEST_F(CompressedSetTest, EmptyTrue) {
  CompressedSet empty;
  EXPECT_TRUE(empty.empty());
}

TEST_F(CompressedSetTest, FalseBitSet) {
  CompressedSet has_bitset("00101010", {});
  EXPECT_FALSE(has_bitset.empty());
}

TEST_F(CompressedSetTest, FalseRanges) {
  CompressedSet has_ranges("", {{1, 2}});
  EXPECT_FALSE(has_ranges.empty());
}

TEST_F(CompressedSetTest, Decode) {
  string bytes("010001010");
  range_vector ranges{{0, 256}};
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeBytes(bytes)));
  cbor_item_unique_ptr ranges_bytestring = empty_cbor_ptr();
  StatusCode sc = RangeList::Encode(ranges, ranges_bytestring);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 1, move_out(ranges_bytestring));

  string old_bytes("9999999");
  range_vector old_ranges{{2, 4}, {4, 8}};

  CompressedSet result(bytes, ranges);

  sc = CompressedSet::Decode(*map, result);
  ASSERT_EQ(sc, StatusCode::kOk);
}

TEST_F(CompressedSetTest, DecodeNotMap) {
  cbor_item_unique_ptr string = make_cbor_string("err");
  CompressedSet result{"orig", {}};
  CompressedSet expected(result);

  StatusCode sc = CompressedSet::Decode(*string, result);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  EXPECT_EQ(result, expected);
}

TEST_F(CompressedSetTest, DecodeNotDefinateMap) {
  cbor_item_unique_ptr map = wrap_cbor_item(cbor_new_indefinite_map());
  cbor_item_unique_ptr ranges_bytestring = empty_cbor_ptr();
  StatusCode sc = RangeList::Encode({{0, 256}}, ranges_bytestring);
  ASSERT_EQ(sc, StatusCode::kOk);
  cbor_map_add(map.get(),
               cbor_pair{.key = cbor_move(CborUtils::EncodeInt(0)),
                         .value = cbor_move(CborUtils::EncodeBytes("1010"))});
  cbor_map_add(map.get(), cbor_pair{.key = cbor_move(CborUtils::EncodeInt(1)),
                                    .value = move_out(ranges_bytestring)});

  CompressedSet result{"orig", {}};
  CompressedSet expected(result);

  sc = CompressedSet::Decode(*map, result);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  EXPECT_EQ(result, expected);
}

TEST_F(CompressedSetTest, DecodeInvalidBytes) {
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("not-bytes")));
  cbor_item_unique_ptr ranges_bytestring = empty_cbor_ptr();
  StatusCode sc = RangeList::Encode({{0, 256}}, ranges_bytestring);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 1, move_out(ranges_bytestring));

  CompressedSet result{"orig", {}};
  CompressedSet expected(result);

  sc = CompressedSet::Decode(*map, result);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  EXPECT_EQ(result, expected);
}

TEST_F(CompressedSetTest, DecodeInvalidRanges) {
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeBytes("0101010")));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeString("err")));

  CompressedSet result{"orig", {}};
  CompressedSet expected(result);

  StatusCode sc = CompressedSet::Decode(*map, result);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  EXPECT_EQ(result, expected);
}

TEST_F(CompressedSetTest, Encode) {
  string bytes("1110110");
  range_vector ranges{{1, 10}, {100, 110}, {200, 210}};
  CompressedSet compressed_set(bytes, ranges);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = compressed_set.Encode(result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result.get(), nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 2);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*result, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  string result_bytes;
  sc = CborUtils::DecodeBytes(*field, result_bytes);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_bytes, bytes);

  sc = CborUtils::GetField(*result, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  range_vector range_results;
  sc = RangeList::Decode(*field, range_results);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(range_results, ranges);
}

TEST_F(CompressedSetTest, EqualsAndNotEquals) {
  CompressedSet cs("010001010", {{2, 4}, {4, 8}});

  ASSERT_EQ(cs, CompressedSet(cs));
  ASSERT_NE(cs, CompressedSet(cs).SetSparseBitSetBytes("other"));
  ASSERT_NE(cs, CompressedSet(cs).ResetSparseBitSetBytes());
  ASSERT_NE(cs, CompressedSet(cs).SetRanges({{1, 100}}));
  ASSERT_NE(cs, CompressedSet(cs).ResetRanges());
  ASSERT_NE(CompressedSet(), CompressedSet("", {}));
}

TEST_F(CompressedSetTest, GettersAndSetters) {
  CompressedSet cs;

  // Initially empty.
  EXPECT_FALSE(cs.HasSparseBitSetBytes());
  EXPECT_FALSE(cs.HasRanges());

  // Default values.
  EXPECT_EQ(cs.SparseBitSetBytes(), "");
  EXPECT_EQ(cs.Ranges(), range_vector{});

  // Now set with default values.
  cs.SetSparseBitSetBytes("");
  cs.SetRanges(range_vector{});

  // Not empty anymore.
  EXPECT_TRUE(cs.HasSparseBitSetBytes());
  EXPECT_TRUE(cs.HasRanges());

  // Double check values.
  EXPECT_EQ(cs.SparseBitSetBytes(), "");
  EXPECT_EQ(cs.Ranges(), range_vector{});

  // Use normal/real values.
  cs.SetSparseBitSetBytes("data-bytes");
  range_vector ranges{{1, 2}, {3, 4}};
  cs.SetRanges(ranges);
  //  vector<int32_t> remapping{1, 5, 10, 20};

  // Still not empty.
  EXPECT_TRUE(cs.HasSparseBitSetBytes());
  EXPECT_TRUE(cs.HasRanges());

  // Double check values.
  EXPECT_EQ(cs.SparseBitSetBytes(), "data-bytes");
  EXPECT_EQ(cs.Ranges(), ranges);

  // Now reset the fields.
  cs.ResetSparseBitSetBytes().ResetRanges();

  // Values are gone.
  EXPECT_FALSE(cs.HasSparseBitSetBytes());
  EXPECT_FALSE(cs.HasRanges());

  // Default values.
  EXPECT_EQ(cs.SparseBitSetBytes(), "");
  EXPECT_EQ(cs.Ranges(), range_vector{});
}

TEST_F(CompressedSetTest, SetCompressedSetFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CompressedSet cs("data", {});
  optional<CompressedSet> opt(cs);

  StatusCode sc = CompressedSet::SetCompressedSetField(*map, 0, cs);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CompressedSet result;
  sc = CompressedSet::Decode(*field, result);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, cs);
}

TEST_F(CompressedSetTest, SetCompressedSetFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<CompressedSet> cs;

  StatusCode sc = CompressedSet::SetCompressedSetField(*map, 0, cs);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CompressedSetTest, GetCompressedSetField) {
  CompressedSet expected("some-data", {{1, 100}});
  cbor_item_unique_ptr cs_map = empty_cbor_ptr();
  StatusCode sc = expected.Encode(cs_map);
  ASSERT_EQ(sc, StatusCode::kOk);
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, move_out(cs_map));
  optional<CompressedSet> result;

  sc = CompressedSet::GetCompressedSetField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, expected);
}

TEST_F(CompressedSetTest, GetCompressedSetFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<CompressedSet> result;

  StatusCode sc = CompressedSet::GetCompressedSetField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(result.has_value());
}

TEST_F(CompressedSetTest, GetCompressedSetFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("bad")));
  optional<CompressedSet> result;

  StatusCode sc = CompressedSet::GetCompressedSetField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(CompressedSetTest, AddRanges) {
  CompressedSet cs;
  ASSERT_FALSE(cs.HasRanges());
  ASSERT_TRUE(cs.Ranges().empty());
  cs.AddRange(range{5, 10});
  ASSERT_TRUE(cs.HasRanges());
  range_vector expected1{{5, 10}};
  ASSERT_EQ(cs.Ranges(), expected1);
  cs.AddRange(range{20, 100});
  range_vector expected2{{5, 10}, {20, 100}};
  ASSERT_EQ(cs.Ranges(), expected2);
  cs.AddRange(range{1000, 2000});
  ASSERT_TRUE(cs.HasRanges());
  range_vector expected3{{5, 10}, {20, 100}, {1000, 2000}};
  ASSERT_EQ(cs.Ranges(), expected3);
}

TEST_F(CompressedSetTest, ToString) {
  CompressedSet cs;
  ASSERT_EQ(cs.ToString(), "{}");
  cs.AddRange(range{1, 2});
  ASSERT_EQ(cs.ToString(), "{[1-2]}");
  cs.AddRange(range{3, 4});
  ASSERT_EQ(cs.ToString(), "{[1-2],[3-4]}");
  cs.SetSparseBitSetBytes("foo");
  ASSERT_EQ(cs.ToString(), "{[1-2],[3-4](w/bitset)}");
}

}  // namespace patch_subset::cbor
