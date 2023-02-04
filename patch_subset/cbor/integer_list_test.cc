#include "patch_subset/cbor/integer_list.h"

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::Status;
using absl::StrFormat;
using absl::string_view;
using std::optional;
using std::string;
using std::vector;

class IntegerListTest : public ::testing::Test {};

string hex(const cbor_item_unique_ptr& bytestring);
cbor_item_unique_ptr build_bytes(vector<uint8_t> bytes);

TEST_F(IntegerListTest, Encode) {
  vector<int32_t> input{2, 3, 0};
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::Encode(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(hex(bytestring), "04 02 05");
  // delta 2 --> 4, delta 1 --> 2, delta -3 --> 5.
}

TEST_F(IntegerListTest, Decode) {
  cbor_item_unique_ptr bytestring = build_bytes(vector<uint8_t>{4, 2, 5});
  vector<int32_t> results;
  // Zig zag 4, 2, 5 --> deltas 2, 1, -3 --> list 2, 3, 0.
  vector<int32_t> expected{2, 3, 0};

  Status sc = IntegerList::Decode(*bytestring, results);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(results, expected);
}

TEST_F(IntegerListTest, OneBytePerInt) {
  vector<int32_t> input;
  const int size = 1000;
  int current = 0;
  for (int i = 0; i < size; i++) {
    input.push_back(current);
    current += i % 63;  // 63 --> zig zag 126 = largest 1 byte positive int.
  }
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::Encode(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  // Exactly 1 byte per int.
  ASSERT_EQ(cbor_bytestring_length(bytestring.get()), size);
}

TEST_F(IntegerListTest, TwoBytesPerInt) {
  vector<int32_t> input;
  const int size = 1000;
  int current = 64;
  for (int i = 0; i < size; i++) {
    input.push_back(current);
    // 64 --> zig zag 128 = smallest 2 byte positive int.
    // 8191 --> zig zag 16382 = largest 2 byte positive int.
    current += 64 + (i % (8191 - 64));
  }
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::Encode(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  // Exactly 2 bytes per int.
  ASSERT_EQ(cbor_bytestring_length(bytestring.get()), size * 2);
}

TEST_F(IntegerListTest, SortedEncode) {
  vector<int32_t> input{2, 3, 10};
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::EncodeSorted(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  // Deltas without zig-zag encoding.
  ASSERT_EQ(hex(bytestring), "02 01 07");
}

TEST_F(IntegerListTest, SortedEncodeStartsPositive) {
  vector<int32_t> input{-1, 3, 10};
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::EncodeSorted(input, bytestring);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(IntegerListTest, SortedEncodeIncreasing) {
  vector<int32_t> input{1, 3, 10, 9};
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::EncodeSorted(input, bytestring);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(IntegerListTest, SortedDecode) {
  cbor_item_unique_ptr bytestring = build_bytes(vector<uint8_t>{2, 1, 7});
  vector<int32_t> results;
  vector<int32_t> expected{2, 3, 10};

  Status sc = IntegerList::DecodeSorted(*bytestring, results);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(results, expected);
}

TEST_F(IntegerListTest, SortedDecodeUniquePtr) {
  cbor_item_unique_ptr bytestring = build_bytes(vector<uint8_t>{2, 1, 7});
  vector<int32_t> results;
  vector<int32_t> expected{2, 3, 10};

  Status sc = IntegerList::DecodeSorted(*bytestring, results);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(results, expected);
}

TEST_F(IntegerListTest, SortedOneBytePerInt) {
  vector<int32_t> input;
  const int size = 1000;
  int current = 0;
  for (int i = 0; i < size; i++) {
    input.push_back(current);
    current += i % 127;  // 127 = largest 1 byte positive int.
  }
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::EncodeSorted(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  // Exactly 1 byte per int.
  ASSERT_EQ(cbor_bytestring_length(bytestring.get()), size);
}

TEST_F(IntegerListTest, SortedTwoBytesPerInt) {
  vector<int32_t> input;
  const int size = 1000;
  int current = 128;
  for (int i = 0; i < size; i++) {
    input.push_back(current);
    // 128 = smallest 2 byte positive int.
    // 16382 = largest 2 byte positive int.
    current += 128 + (i % (16382 - 128));
  }
  cbor_item_unique_ptr bytestring = empty_cbor_ptr();

  Status sc = IntegerList::EncodeSorted(input, bytestring);

  ASSERT_EQ(sc, absl::OkStatus());
  // Exactly 2 bytes per int.
  ASSERT_EQ(cbor_bytestring_length(bytestring.get()), size * 2);
}

TEST_F(IntegerListTest, IsEmptyTrue) {
  cbor_item_unique_ptr bytes = make_cbor_bytestring("");
  bool result;

  Status sc = IntegerList::IsEmpty(*bytes, &result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_TRUE(result);
}

TEST_F(IntegerListTest, IsEmptyFalse) {
  string buffer{"ABCD"};
  cbor_item_unique_ptr bytes = make_cbor_bytestring(buffer);
  bool result;

  Status sc = IntegerList::IsEmpty(*bytes, &result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result);
}

TEST_F(IntegerListTest, GetIntegerListField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<int32_t> expected{101, 200, 1000, 500, 20, 0};
  cbor_item_unique_ptr value = empty_cbor_ptr();
  Status sc = IntegerList::Encode(expected, value);
  ASSERT_EQ(sc, absl::OkStatus());
  CborUtils::SetField(*map, 0, move_out(value));
  optional<vector<int32_t>> result;

  sc = IntegerList::GetIntegerListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, expected);
}

TEST_F(IntegerListTest, GetIntegerListFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<vector<int32_t>> result({1, 2, 3});

  Status sc = IntegerList::GetIntegerListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(IntegerListTest, GetIntegerListFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("bad")));
  optional<vector<int32_t>> result({-1, -2});

  Status sc = IntegerList::GetIntegerListField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, vector<int32_t>({-1, -2}));
}

string hex(const cbor_item_unique_ptr& bytestring) {
  uint8_t* handle = cbor_bytestring_handle(bytestring.get());
  size_t size = cbor_bytestring_length(bytestring.get());
  string s;
  for (size_t i = 0; i < size; i++) {
    s += StrFormat("%02x", handle[i]);
    if (i < size - 1) {
      s += " ";
    }
  }
  return s;
}

cbor_item_unique_ptr build_bytes(vector<uint8_t> bytes) {
  auto buffer = std::make_unique<uint8_t[]>(bytes.size());
  for (size_t i = 0; i < bytes.size(); i++) {
    buffer.get()[i] = bytes[i];
  }
  string_view sv{(char*)buffer.get(), bytes.size()};
  return make_cbor_bytestring(sv);
}
}  // namespace patch_subset::cbor
