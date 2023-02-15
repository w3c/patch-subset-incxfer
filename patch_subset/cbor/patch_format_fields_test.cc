#include "patch_subset/cbor/patch_format_fields.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/array.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

using absl::Status;
using patch_subset::PatchFormat;
using std::optional;
using std::vector;

class PatchFormatFieldsTest : public ::testing::Test {};

TEST_F(PatchFormatFieldsTest, ToPatchFormatBrotli) {
  auto result = static_cast<PatchFormat>(-1);
  Status sc = PatchFormatFields::ToPatchFormat(0, &result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, PatchFormat::VCDIFF);
}

TEST_F(PatchFormatFieldsTest, ToPatchFormatVcdiff) {
  auto result = static_cast<PatchFormat>(-1);
  Status sc = PatchFormatFields::ToPatchFormat(1, &result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, ToPatchFormatInvalid) {
  auto result = static_cast<PatchFormat>(-1);
  Status sc = PatchFormatFields::ToPatchFormat(2, &result);
  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, -1);
}

TEST_F(PatchFormatFieldsTest, Encode) {
  vector<PatchFormat> input{PatchFormat::BROTLI_SHARED_DICT,
                            PatchFormat::VCDIFF};
  cbor_item_unique_ptr result = empty_cbor_ptr();

  Status sc = PatchFormatFields::Encode(input, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(result.get(), nullptr);

  vector<uint64_t> result_ints;
  sc = Array::Decode(*result, result_ints);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<uint64_t> expected_ints{PatchFormat::BROTLI_SHARED_DICT,
                                 PatchFormat::VCDIFF};
  ASSERT_EQ(result_ints, expected_ints);
}

TEST_F(PatchFormatFieldsTest, Decode) {
  cbor_item_unique_ptr bytes = empty_cbor_ptr();
  ASSERT_EQ(Array::Encode({1, 0}, bytes), absl::OkStatus());
  vector<PatchFormat> results;

  Status sc = PatchFormatFields::Decode(*bytes, results);

  ASSERT_EQ(sc, absl::OkStatus());
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT,
                               PatchFormat::VCDIFF};
  ASSERT_EQ(results, expected);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<PatchFormat> pf(PatchFormat::VCDIFF);

  Status sc = PatchFormatFields::SetPatchFormatField(*map, 0, pf);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  int result;
  sc = CborUtils::DecodeInt(*field, &result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, PatchFormat::VCDIFF);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<PatchFormat> pf;

  Status sc = PatchFormatFields::SetPatchFormatField(*map, 0, pf);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr field = empty_cbor_ptr();
  ASSERT_EQ(
      CborUtils::SetField(
          *map, 0,
          cbor_move(CborUtils::EncodeInt(PatchFormat::BROTLI_SHARED_DICT))),
      absl::OkStatus());
  optional<PatchFormat> result;

  Status sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<PatchFormat> result(PatchFormat::BROTLI_SHARED_DICT);

  Status sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("err"))),
      absl::OkStatus());
  optional<PatchFormat> result(PatchFormat::BROTLI_SHARED_DICT);

  Status sc = PatchFormatFields::GetPatchFormatField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatsListFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<PatchFormat> pfv{PatchFormat::VCDIFF};
  optional<vector<PatchFormat>> pf(pfv);

  Status sc = PatchFormatFields::SetPatchFormatsListField(*map, 0, pf);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<uint64_t> result;
  sc = Array::Decode(*field, result);
  ASSERT_EQ(sc, absl::OkStatus());
  vector<uint64_t> expected{PatchFormat::VCDIFF};
  ASSERT_EQ(result, expected);
}

TEST_F(PatchFormatFieldsTest, SetPatchFormatsListFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<vector<PatchFormat>> pf;

  Status sc = PatchFormatFields::SetPatchFormatsListField(*map, 0, pf);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  vector<uint64_t> formats{PatchFormat::BROTLI_SHARED_DICT,
                           PatchFormat::VCDIFF};
  cbor_item_unique_ptr field = empty_cbor_ptr();
  ASSERT_EQ(Array::Encode(formats, field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, 0, move_out(field)), absl::OkStatus());

  optional<vector<PatchFormat>> result;

  Status sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT,
                               PatchFormat::VCDIFF};
  ASSERT_EQ(result.value(), expected);
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  vector<PatchFormat> resultv{PatchFormat::BROTLI_SHARED_DICT};
  optional<vector<PatchFormat>> result(resultv);

  Status sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(PatchFormatFieldsTest, GetPatchFormatsListFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("err"))),
      absl::OkStatus());
  vector<PatchFormat> resultv{PatchFormat::BROTLI_SHARED_DICT};
  optional<vector<PatchFormat>> result(resultv);

  Status sc = PatchFormatFields::GetPatchFormatsListField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT};
  ASSERT_EQ(result.value(), expected);
}

}  // namespace patch_subset::cbor
