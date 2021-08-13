#include "patch_subset/cbor/patch_response.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using std::string;
using std::vector;

class PatchResponseTest : public ::testing::Test {};

TEST_F(PatchResponseTest, EmptyConstructor) {
  PatchResponse response;

  EXPECT_EQ(response.GetProtocolVersion(), ProtocolVersion::ONE);
  // TODO: Add UNKNOWN value.
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);
  EXPECT_TRUE(response.Patch().empty());
  EXPECT_TRUE(response.Replacement().empty());
  EXPECT_EQ(response.OriginalFontChecksum(), 0);
  EXPECT_EQ(response.PatchedChecksum(), 0);
  EXPECT_TRUE(response.CodepointOrdering().empty());
  EXPECT_EQ(response.OrderingChecksum(), 0);
}

TEST_F(PatchResponseTest, Constructor) {
  PatchFormat patch_format = PatchFormat::VCDIFF;
  string patch = "patch-data";
  string replacement = "replacement-data";
  uint64_t original_font_checksum = 1234;
  uint64_t patched_checksum = 2345;
  vector<int32_t> codepoint_ordering{1, 5, 10, 20};
  uint64_t ordering_checksum = 3456;

  PatchResponse response(ProtocolVersion::ONE, patch_format, patch, replacement,
                         original_font_checksum, patched_checksum,
                         codepoint_ordering, ordering_checksum);

  EXPECT_EQ(response.GetProtocolVersion(), ProtocolVersion::ONE);
  EXPECT_EQ(response.GetPatchFormat(), patch_format);
  EXPECT_EQ(response.Patch(), patch);
  EXPECT_EQ(response.Replacement(), replacement);
  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), patched_checksum);
  EXPECT_EQ(response.CodepointOrdering(), codepoint_ordering);
  EXPECT_EQ(response.OrderingChecksum(), ordering_checksum);
}

TEST_F(PatchResponseTest, CopyConstructor) {
  PatchFormat patch_format = PatchFormat::VCDIFF;
  string patch = "patch-data";
  string replacement = "replacement-data";
  uint64_t original_font_checksum = 1234;
  uint64_t patched_checksum = 2345;
  vector<int32_t> codepoint_ordering{1, 5, 10, 20};
  uint64_t ordering_checksum = 3456;
  PatchResponse response(ProtocolVersion::ONE, patch_format, patch, replacement,
                         original_font_checksum, patched_checksum,
                         codepoint_ordering, ordering_checksum);

  EXPECT_EQ(PatchResponse(response), response);
}

TEST_F(PatchResponseTest, MoveConstructor) {
  PatchFormat patch_format = PatchFormat::VCDIFF;
  string patch = "patch-data";
  string replacement = "replacement-data";
  uint64_t original_font_checksum = 1234;
  uint64_t patched_checksum = 2345;
  vector<int32_t> codepoint_ordering{1, 5, 10, 20};
  uint64_t ordering_checksum = 3456;
  PatchResponse origional(ProtocolVersion::ONE, patch_format, patch,
                          replacement, original_font_checksum, patched_checksum,
                          codepoint_ordering, ordering_checksum);

  PatchResponse moved = std::move(origional);

  EXPECT_EQ(moved.GetProtocolVersion(), ProtocolVersion::ONE);
  EXPECT_EQ(moved.GetPatchFormat(), patch_format);
  EXPECT_EQ(moved.Patch(), patch);
  EXPECT_EQ(moved.Replacement(), replacement);
  EXPECT_EQ(moved.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(moved.PatchedChecksum(), patched_checksum);
  EXPECT_EQ(moved.CodepointOrdering(), codepoint_ordering);
  EXPECT_EQ(moved.OrderingChecksum(), ordering_checksum);
}

TEST_F(PatchResponseTest, Encode) {
  PatchFormat patch_format = PatchFormat::BROTLI_SHARED_DICT;
  string patch = "patch-data2";
  string replacement = "replacement-data2";
  uint64_t original_font_checksum = 2345;
  uint64_t patched_checksum = 3456;
  vector<int32_t> codepoint_ordering{1, 2, 3, 4, 5};
  uint64_t ordering_checksum = 4567;

  PatchResponse response(ProtocolVersion::ONE, patch_format, patch, replacement,
                         original_font_checksum, patched_checksum,
                         codepoint_ordering, ordering_checksum);
  cbor_item_unique_ptr map = empty_cbor_ptr();
  StatusCode sc = response.Encode(map);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(map.get(), nullptr);
  cbor_item_unique_ptr field = empty_cbor_ptr();

  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  int version;
  sc = CborUtils::DecodeInt(*field, &version);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(version, response.GetProtocolVersion());

  sc = CborUtils::GetField(*map, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  int format;
  sc = CborUtils::DecodeInt(*field, &format);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(format, response.GetPatchFormat());

  sc = CborUtils::GetField(*map, 2, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  string data;
  sc = CborUtils::DecodeString(*field, data);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(data, response.Patch());

  sc = CborUtils::GetField(*map, 3, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeString(*field, data);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(data, response.Replacement());

  sc = CborUtils::GetField(*map, 4, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  uint64_t checksum;
  sc = CborUtils::DecodeUInt64(*field, &checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(checksum, response.OriginalFontChecksum());

  sc = CborUtils::GetField(*map, 5, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeUInt64(*field, &checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(checksum, response.PatchedChecksum());

  sc = CborUtils::GetField(*map, 6, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<int32_t> order;
  sc = IntegerList::Decode(*field, order);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(order, response.CodepointOrdering());

  sc = CborUtils::GetField(*map, 7, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeUInt64(*field, &checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(checksum, response.OrderingChecksum());
}

TEST_F(PatchResponseTest, Decode) {
  PatchFormat patch_format = PatchFormat::BROTLI_SHARED_DICT;
  string patch = "patch-data2";
  string replacement = "replacement-data2";
  uint64_t original_font_checksum = 2345;
  uint64_t patched_checksum = 3456;
  vector<int32_t> codepoint_ordering{1, 2, 3, 4, 5};
  uint64_t ordering_checksum = 4567;

  PatchResponse expected(ProtocolVersion::ONE, patch_format, patch, replacement,
                         original_font_checksum, patched_checksum,
                         codepoint_ordering, ordering_checksum);
  cbor_item_unique_ptr map = make_cbor_map(8);
  cbor_item_unique_ptr type = empty_cbor_ptr();

  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeInt(0)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeInt(patch_format)));
  CborUtils::SetField(*map, 2, cbor_move(CborUtils::EncodeString(patch)));
  CborUtils::SetField(*map, 3, cbor_move(CborUtils::EncodeString(replacement)));
  CborUtils::SetField(
      *map, 4, cbor_move(CborUtils::EncodeUInt64(original_font_checksum)));
  CborUtils::SetField(*map, 5,
                      cbor_move(CborUtils::EncodeUInt64(patched_checksum)));
  cbor_item_unique_ptr remapping = empty_cbor_ptr();
  IntegerList::Encode(codepoint_ordering, remapping);
  CborUtils::SetField(*map, 6, move_out(remapping));
  CborUtils::SetField(*map, 7,
                      cbor_move(CborUtils::EncodeUInt64(ordering_checksum)));
  PatchResponse response;

  StatusCode sc = PatchResponse::Decode(*map, response);

  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(response, expected);
}

TEST_F(PatchResponseTest, GettersAndSetters) {
  PatchResponse response;

  EXPECT_FALSE(response.HasProtocolVersion());
  EXPECT_EQ(response.GetProtocolVersion(), ProtocolVersion::ONE);
  response.SetProtocolVersion(ProtocolVersion::ONE);
  EXPECT_TRUE(response.HasProtocolVersion());
  EXPECT_EQ(response.GetProtocolVersion(), ProtocolVersion::ONE);
  response.ResetProtocolVersion();
  EXPECT_FALSE(response.HasProtocolVersion());

  EXPECT_FALSE(response.HasPatchFormat());
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);
  response.SetPatchFormat(PatchFormat::BROTLI_SHARED_DICT);
  EXPECT_TRUE(response.HasPatchFormat());
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);
  response.ResetPatchFormat();
  EXPECT_FALSE(response.HasPatchFormat());

  EXPECT_FALSE(response.HasPatch());
  EXPECT_EQ(response.Patch(), "");
  response.SetPatch("data");
  EXPECT_TRUE(response.HasPatch());
  EXPECT_EQ(response.Patch(), "data");
  response.ResetPatch();
  EXPECT_FALSE(response.HasPatch());

  EXPECT_FALSE(response.HasReplacement());
  EXPECT_EQ(response.Replacement(), "");
  response.SetReplacement("data");
  EXPECT_TRUE(response.HasReplacement());
  EXPECT_EQ(response.Replacement(), "data");
  response.ResetReplacement();
  EXPECT_FALSE(response.HasReplacement());

  EXPECT_FALSE(response.HasOriginalFontChecksum());
  EXPECT_EQ(response.OriginalFontChecksum(), 0);
  response.SetOriginalFontChecksum(42);
  EXPECT_TRUE(response.HasOriginalFontChecksum());
  EXPECT_EQ(response.OriginalFontChecksum(), 42);
  response.ResetOriginalFontChecksum();
  EXPECT_FALSE(response.HasOriginalFontChecksum());

  EXPECT_FALSE(response.HasPatchedChecksum());
  EXPECT_EQ(response.PatchedChecksum(), 0);
  response.SetPatchedChecksum(42);
  EXPECT_TRUE(response.HasPatchedChecksum());
  EXPECT_EQ(response.PatchedChecksum(), 42);
  response.ResetPatchedChecksum();
  EXPECT_FALSE(response.HasPatchedChecksum());

  EXPECT_FALSE(response.HasCodepointOrdering());
  EXPECT_TRUE(response.CodepointOrdering().empty());
  vector<int32_t> ordering{3, 2, 1};
  response.SetCodepointOrdering({3, 2, 1});
  EXPECT_TRUE(response.HasCodepointOrdering());
  EXPECT_EQ(response.CodepointOrdering(), ordering);
  response.ResetCodepointOrdering();
  EXPECT_FALSE(response.HasCodepointOrdering());

  EXPECT_FALSE(response.HasOrderingChecksum());
  EXPECT_EQ(response.OrderingChecksum(), 0);
  response.SetOrderingChecksum(42);
  EXPECT_TRUE(response.HasOrderingChecksum());
  EXPECT_EQ(response.OrderingChecksum(), 42);
  response.ResetOrderingChecksum();
  EXPECT_FALSE(response.HasOrderingChecksum());
}

TEST_F(PatchResponseTest, EqualsAndNotEquals) {
  PatchResponse response(ProtocolVersion::ONE, PatchFormat::VCDIFF,
                         "patch-data", "replacement-data", 1234, 2345,
                         {1, 5, 10, 20}, 3456);

  EXPECT_EQ(response, PatchResponse(response));
  EXPECT_NE(response, PatchResponse(response).SetProtocolVersion(
                          static_cast<ProtocolVersion>(-1)));
  EXPECT_NE(response, PatchResponse(response).ResetProtocolVersion());
  EXPECT_NE(response, PatchResponse(response).SetPatchFormat(
                          PatchFormat::BROTLI_SHARED_DICT));
  EXPECT_NE(response, PatchResponse(response).ResetPatchFormat());
  EXPECT_NE(response, PatchResponse(response).SetPatch("other"));
  EXPECT_NE(response, PatchResponse(response).ResetPatch());
  EXPECT_NE(response, PatchResponse(response).SetReplacement("other"));
  EXPECT_NE(response, PatchResponse(response).ResetReplacement());
  EXPECT_NE(response, PatchResponse(response).SetOriginalFontChecksum(42));
  EXPECT_NE(response, PatchResponse(response).ResetOriginalFontChecksum());
  EXPECT_NE(response, PatchResponse(response).SetPatchedChecksum(42));
  EXPECT_NE(response, PatchResponse(response).ResetPatchedChecksum());
  EXPECT_NE(response, PatchResponse(response).SetCodepointOrdering({}));
  EXPECT_NE(response, PatchResponse(response).ResetCodepointOrdering());
  EXPECT_NE(response, PatchResponse(response).SetOrderingChecksum(42));
  EXPECT_NE(response, PatchResponse(response).ResetOrderingChecksum());
}

}  // namespace patch_subset::cbor
