#include "patch_subset/cbor/patch_format_fields.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

using patch_subset::PatchFormat;
using std::optional;
using std::vector;

class PatchFormatFieldsTest : public ::testing::Test {};

TEST_F(PatchFormatFieldsTest, ToPatchFormatBrotli) {
  auto result = static_cast<PatchFormat>(-1);
  StatusCode sc = PatchFormatFields::ToPatchFormat(0, &result);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, PatchFormat::VCDIFF);
}

TEST_F(PatchFormatFieldsTest, ToPatchFormatVcdiff) {
  auto result = static_cast<PatchFormat>(-1);
  StatusCode sc = PatchFormatFields::ToPatchFormat(1, &result);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, ToPatchFormatInvalid) {
  auto result = static_cast<PatchFormat>(-1);
  StatusCode sc = PatchFormatFields::ToPatchFormat(2, &result);
  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  ASSERT_EQ(result, -1);
}

TEST_F(PatchFormatFieldsTest, Encode) {
  vector<PatchFormat> input{PatchFormat::BROTLI_SHARED_DICT,
                            PatchFormat::VCDIFF};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = PatchFormatFields::Encode(input, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result.get(), nullptr);

  vector<int32_t> result_ints;
  sc = IntegerList::Decode(*result, result_ints);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<int32_t> expected_ints{PatchFormat::BROTLI_SHARED_DICT,
                                PatchFormat::VCDIFF};
  ASSERT_EQ(result_ints, expected_ints);
}

TEST_F(PatchFormatFieldsTest, Decode) {
  cbor_item_unique_ptr bytes = empty_cbor_ptr();
  IntegerList::Encode({1, 0}, bytes);
  vector<PatchFormat> results;

  StatusCode sc = PatchFormatFields::Decode(*bytes, results);

  ASSERT_EQ(sc, StatusCode::kOk);
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT,
                               PatchFormat::VCDIFF};
  ASSERT_EQ(results, expected);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<PatchFormat> pf(PatchFormat::VCDIFF);

  StatusCode sc = PatchFormatFields::SetPatchFormatField(*map, 0, pf);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  int result;
  sc = CborUtils::DecodeInt(*field, &result);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, PatchFormat::VCDIFF);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<PatchFormat> pf;

  StatusCode sc = PatchFormatFields::SetPatchFormatField(*map, 0, pf);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr field = empty_cbor_ptr();
  CborUtils::SetField(
      *map, 0,
      cbor_move(CborUtils::EncodeInt(PatchFormat::BROTLI_SHARED_DICT)));
  optional<PatchFormat> result;

  StatusCode sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<PatchFormat> result(PatchFormat::BROTLI_SHARED_DICT);

  StatusCode sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(result.has_value());
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("err")));
  optional<PatchFormat> result(PatchFormat::BROTLI_SHARED_DICT);

  StatusCode sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatsListFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<PatchFormat> pfv{PatchFormat::VCDIFF};
  optional<vector<PatchFormat>> pf(pfv);

  StatusCode sc = PatchFormatFields::SetPatchFormatsListField(*map, 0, pf);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<int32_t> result;
  sc = IntegerList::Decode(*field, result);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<int32_t> expected{PatchFormat::VCDIFF};
  ASSERT_EQ(result, expected);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatsListFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<vector<PatchFormat>> pf;

  StatusCode sc = PatchFormatFields::SetPatchFormatsListField(*map, 0, pf);
  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<int32_t> formats{PatchFormat::BROTLI_SHARED_DICT, PatchFormat::VCDIFF};
  cbor_item_unique_ptr field = empty_cbor_ptr();
  IntegerList::Encode(formats, field);
  CborUtils::SetField(*map, 0, move_out(field));
  optional<vector<PatchFormat>> result;

  StatusCode sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT,
                               PatchFormat::VCDIFF};
  ASSERT_EQ(result.value(), expected);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  vector<PatchFormat> resultv{PatchFormat::BROTLI_SHARED_DICT};
  optional<vector<PatchFormat>> result(resultv);

  StatusCode sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(result.has_value());
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("err")));
  vector<PatchFormat> resultv{PatchFormat::BROTLI_SHARED_DICT};
  optional<vector<PatchFormat>> result(resultv);

  StatusCode sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT};
  ASSERT_EQ(result.value(), expected);
}

}  // namespace patch_subset::cbor
