#include "patch_subset/cbor/array.h"

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::StatusCode;
using std::optional;
using std::vector;

class ArrayTest : public ::testing::Test {};

static void check_cbor_array_equal(const cbor_item_t& cbor_array,
                                   const vector<uint64_t>& array) {
  ASSERT_TRUE(cbor_array_is_definite(&cbor_array));
  ASSERT_EQ(cbor_array_size(&cbor_array), array.size());
  for (unsigned i = 0; i < array.size(); i++) {
    uint64_t value;
    cbor_item_unique_ptr element =
        wrap_cbor_item(cbor_array_get(&cbor_array, i));
    ASSERT_EQ(StatusCode::kOk, CborUtils::DecodeUInt64(*element, &value));
    ASSERT_EQ(array[i], value);
  }
}

TEST_F(ArrayTest, EncodeEmpty) {
  vector<uint64_t> input;
  cbor_item_unique_ptr array = empty_cbor_ptr();

  StatusCode sc = Array::Encode(input, array);

  ASSERT_EQ(sc, StatusCode::kOk);
  check_cbor_array_equal(*array, input);
}

TEST_F(ArrayTest, Encode) {
  vector<uint64_t> input{2, 3, 0};
  cbor_item_unique_ptr array = empty_cbor_ptr();

  StatusCode sc = Array::Encode(input, array);

  ASSERT_EQ(sc, StatusCode::kOk);
  check_cbor_array_equal(*array, input);
}

TEST_F(ArrayTest, Decode) {
  cbor_item_unique_ptr bytestring = make_cbor_array(3);
  ASSERT_TRUE(cbor_array_push(bytestring.get(),
                              cbor_move(CborUtils::EncodeUInt64(13))));
  ASSERT_TRUE(cbor_array_push(bytestring.get(),
                              cbor_move(CborUtils::EncodeUInt64(12759))));
  ASSERT_TRUE(
      cbor_array_push(bytestring.get(), cbor_move(CborUtils::EncodeUInt64(0))));

  vector<uint64_t> expected{13, 12759, 0};

  vector<uint64_t> result;
  StatusCode sc = Array::Decode(*bytestring, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, expected);
}

TEST_F(ArrayTest, SetIntegerArrayField) {
  cbor_item_unique_ptr map = make_cbor_map(1);

  vector<uint64_t> data{101, 200, 1000, 500, 20, 0};
  ASSERT_EQ(StatusCode::kOk, Array::SetArrayField(*map, 42, data));

  ASSERT_EQ(cbor_map_size(map.get()), 1);
  cbor_pair pair = cbor_map_handle(map.get())[0];

  int32_t key;
  CborUtils::DecodeInt(*(pair.key), &key);
  ASSERT_EQ(key, 42);
  check_cbor_array_equal(*(pair.value), data);
}

TEST_F(ArrayTest, GetIntegerArrayField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<uint64_t> expected{101, 200, 1000, 500, 20, 0};
  cbor_item_unique_ptr value = empty_cbor_ptr();
  StatusCode sc = Array::Encode(expected, value);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 0, move_out(value));

  optional<vector<uint64_t>> result;
  sc = Array::GetArrayField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_TRUE(result);
  ASSERT_EQ(*result, expected);
}

TEST_F(ArrayTest, GetIntegerArrayFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);

  optional<vector<uint64_t>> result({1, 2, 3});
  StatusCode sc = Array::GetArrayField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(result.has_value());
}

TEST_F(ArrayTest, GetIntegerArrayFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("bad")));
  optional<vector<uint64_t>> result({1, 2});

  StatusCode sc = Array::GetArrayField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  ASSERT_EQ(*result, vector<uint64_t>({1, 2}));
}

}  // namespace patch_subset::cbor
