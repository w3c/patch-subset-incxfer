#include "patch_subset/cbor/patch_request.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/patch_format_fields.h"

namespace patch_subset::cbor {

class PatchRequestTest : public ::testing::Test {};

using std::string;
using std::vector;

TEST_F(PatchRequestTest, Constructor) {
  vector<PatchFormat> accept_formats{PatchFormat::BROTLI_SHARED_DICT};
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 98989898L;
  uint64_t original_font_checksum = 34343434L;
  uint64_t base_checksum = 12121212L;
  ConnectionSpeed speed = ConnectionSpeed::VERY_FAST;

  PatchRequest request(ProtocolVersion::ONE, accept_formats, codepoints_have,
                       codepoints_needed, indices_have, indices_needed,
                       ordering_checksum, original_font_checksum, base_checksum,
                       speed);

  EXPECT_EQ(request.GetProtocolVersion(), ProtocolVersion::ONE);
  EXPECT_EQ(request.AcceptFormats(), accept_formats);
  EXPECT_EQ(request.CodepointsHave(), codepoints_have);
  EXPECT_EQ(request.CodepointsNeeded(), codepoints_needed);
  EXPECT_EQ(request.IndicesHave(), indices_have);
  EXPECT_EQ(request.IndicesNeeded(), indices_needed);
  EXPECT_EQ(request.OrderingChecksum(), ordering_checksum);
  EXPECT_EQ(request.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(request.BaseChecksum(), base_checksum);
  EXPECT_EQ(request.GetConnectionSpeed(), speed);

  EXPECT_EQ(PatchRequest(request), request);
}

TEST_F(PatchRequestTest, Decode) {
  vector<PatchFormat> accept_formats{PatchFormat::BROTLI_SHARED_DICT};
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 4567;
  uint64_t original_font_checksum = 5678;
  uint64_t base_checksum = 6789;
  ConnectionSpeed connection_speed = ConnectionSpeed::VERY_SLOW;

  PatchRequest expected(ProtocolVersion::ONE, accept_formats, codepoints_have,
                        codepoints_needed, indices_have, indices_needed,
                        ordering_checksum, original_font_checksum,
                        base_checksum, connection_speed);

  cbor_item_unique_ptr map = make_cbor_map(10);
  cbor_item_unique_ptr field = empty_cbor_ptr();
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeUInt64(0)));
  PatchFormatFields::Encode(accept_formats, field);
  CborUtils::SetField(*map, 1, move_out(field));
  codepoints_have.Encode(field);
  CborUtils::SetField(*map, 2, move_out(field));
  codepoints_needed.Encode(field);
  CborUtils::SetField(*map, 3, move_out(field));
  indices_have.Encode(field);
  CborUtils::SetField(*map, 4, move_out(field));
  indices_needed.Encode(field);
  CborUtils::SetField(*map, 5, move_out(field));
  CborUtils::SetField(*map, 6,
                      cbor_move(CborUtils::EncodeUInt64(ordering_checksum)));
  CborUtils::SetField(
      *map, 7, cbor_move(CborUtils::EncodeUInt64(original_font_checksum)));
  CborUtils::SetField(*map, 8,
                      cbor_move(CborUtils::EncodeUInt64(base_checksum)));
  CborUtils::SetField(*map, 9, cbor_move(CborUtils::EncodeInt(1)));
  PatchRequest result;

  StatusCode sc = PatchRequest::Decode(*map, result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result, expected);
}

TEST_F(PatchRequestTest, Encode) {
  vector<PatchFormat> accept_formats{PatchFormat::BROTLI_SHARED_DICT,
                                     static_cast<PatchFormat>(-1)};
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 34343434L;
  uint64_t original_font_checksum = 12121212L;
  uint64_t base_checksum = 98989898L;
  ConnectionSpeed connection_speed = ConnectionSpeed::FAST;
  PatchRequest request(ProtocolVersion::ONE, accept_formats, codepoints_have,
                       codepoints_needed, indices_have, indices_needed,
                       ordering_checksum, original_font_checksum, base_checksum,
                       connection_speed);
  cbor_item_unique_ptr map = empty_cbor_ptr();

  StatusCode sc = request.Encode(map);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_TRUE(cbor_isa_map(map.get()));
  ASSERT_EQ(cbor_map_size(map.get()), 10);
  cbor_item_unique_ptr field = empty_cbor_ptr();

  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  int protocol_version;
  sc = CborUtils::DecodeInt(*field, &protocol_version);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(protocol_version, ProtocolVersion::ONE);

  sc = CborUtils::GetField(*map, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  vector<PatchFormat> result_accept_formats;
  sc = PatchFormatFields::Decode(*field, result_accept_formats);
  // Illegal value was ignored.
  vector<PatchFormat> expected{PatchFormat::BROTLI_SHARED_DICT};
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_accept_formats, expected);

  sc = CborUtils::GetField(*map, 2, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CompressedSet result_codepoints_have;
  sc = CompressedSet::Decode(*field, result_codepoints_have);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_codepoints_have, codepoints_have);

  sc = CborUtils::GetField(*map, 3, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CompressedSet result_codepoints_needed;
  sc = CompressedSet::Decode(*field, result_codepoints_needed);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_codepoints_needed, codepoints_needed);

  sc = CborUtils::GetField(*map, 4, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CompressedSet result_indices_have;
  sc = CompressedSet::Decode(*field, result_indices_have);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_indices_have, indices_have);

  sc = CborUtils::GetField(*map, 5, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CompressedSet result_indices_needed;
  sc = CompressedSet::Decode(*field, result_indices_needed);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_indices_needed, indices_needed);

  sc = CborUtils::GetField(*map, 6, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  uint64_t result_ordering_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_ordering_checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_ordering_checksum, ordering_checksum);

  sc = CborUtils::GetField(*map, 7, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  uint64_t result_original_font_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_original_font_checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_original_font_checksum, original_font_checksum);

  sc = CborUtils::GetField(*map, 8, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  uint64_t result_base_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_base_checksum);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_base_checksum, base_checksum);

  sc = CborUtils::GetField(*map, 9, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  int result_speed;
  sc = CborUtils::DecodeInt(*field, &result_speed);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_speed, connection_speed);
}

TEST_F(PatchRequestTest, GettersAndSetters) {
  PatchRequest request;

  EXPECT_FALSE(request.HasProtocolVersion());
  EXPECT_EQ(request.GetProtocolVersion(), ProtocolVersion::ONE);
  request.SetProtocolVersion(ProtocolVersion::ONE);
  EXPECT_TRUE(request.HasProtocolVersion());
  EXPECT_EQ(request.GetProtocolVersion(), ProtocolVersion::ONE);
  request.ResetProtocolVersion();
  EXPECT_FALSE(request.HasProtocolVersion());

  EXPECT_FALSE(request.HasAcceptFormats());
  EXPECT_TRUE(request.AcceptFormats().empty());
  request.SetAcceptFormats({PatchFormat::VCDIFF});
  EXPECT_TRUE(request.HasAcceptFormats());
  EXPECT_EQ(request.AcceptFormats(), vector<PatchFormat>{PatchFormat::VCDIFF});
  request.ResetAcceptFormats();
  EXPECT_FALSE(request.HasAcceptFormats());

  EXPECT_FALSE(request.HasCodepointsHave());
  EXPECT_EQ(request.CodepointsHave(), CompressedSet());
  request.SetCodepointsHave({"data", {}});
  EXPECT_TRUE(request.HasCodepointsHave());
  EXPECT_EQ(request.CodepointsHave(), CompressedSet("data", {}));
  request.ResetCodepointsHave();
  EXPECT_FALSE(request.HasCodepointsHave());

  EXPECT_FALSE(request.HasCodepointsNeeded());
  EXPECT_EQ(request.CodepointsNeeded(), CompressedSet());
  request.SetCodepointsNeeded({"data", {}});
  EXPECT_TRUE(request.HasCodepointsNeeded());
  EXPECT_EQ(request.CodepointsNeeded(), CompressedSet("data", {}));
  request.ResetCodepointsNeeded();
  EXPECT_FALSE(request.HasCodepointsNeeded());

  EXPECT_FALSE(request.HasIndicesHave());
  EXPECT_EQ(request.IndicesHave(), CompressedSet());
  request.SetIndicesHave({{"data"}, {}});
  EXPECT_TRUE(request.HasIndicesHave());
  EXPECT_EQ(request.IndicesHave(), CompressedSet("data", {}));
  request.ResetIndicesHave();
  EXPECT_FALSE(request.HasIndicesHave());

  EXPECT_FALSE(request.HasIndicesNeeded());
  EXPECT_EQ(request.IndicesNeeded(), CompressedSet());
  request.SetIndicesNeeded({{"data"}, {}});
  EXPECT_TRUE(request.HasIndicesNeeded());
  EXPECT_EQ(request.IndicesNeeded(), CompressedSet("data", {}));
  request.ResetIndicesNeeded();
  EXPECT_FALSE(request.HasIndicesNeeded());

  EXPECT_FALSE(request.HasOrderingChecksum());
  EXPECT_EQ(request.OrderingChecksum(), 0);
  request.SetOrderingChecksum(42);
  EXPECT_TRUE(request.HasOrderingChecksum());
  EXPECT_EQ(request.OrderingChecksum(), 42);
  request.ResetOrderingChecksum();
  EXPECT_FALSE(request.HasOrderingChecksum());

  EXPECT_FALSE(request.HasOriginalFontChecksum());
  EXPECT_EQ(request.OriginalFontChecksum(), 0);
  request.SetOriginalFontChecksum(42);
  EXPECT_TRUE(request.HasOriginalFontChecksum());
  EXPECT_EQ(request.OriginalFontChecksum(), 42);
  request.ResetOriginalFontChecksum();
  EXPECT_FALSE(request.HasOriginalFontChecksum());

  EXPECT_FALSE(request.HasBaseChecksum());
  EXPECT_EQ(request.BaseChecksum(), 0);
  request.SetBaseChecksum(42);
  EXPECT_TRUE(request.HasBaseChecksum());
  EXPECT_EQ(request.BaseChecksum(), 42);
  request.ResetBaseChecksum();
  EXPECT_FALSE(request.HasBaseChecksum());

  EXPECT_FALSE(request.HasConnectionSpeed());
  EXPECT_EQ(request.GetConnectionSpeed(), ConnectionSpeed::AVERAGE);
  request.SetConnectionSpeed(ConnectionSpeed::FAST);
  EXPECT_TRUE(request.HasConnectionSpeed());
  EXPECT_EQ(request.GetConnectionSpeed(), ConnectionSpeed::FAST);
  request.ResetConnectionSpeed();
  EXPECT_FALSE(request.HasConnectionSpeed());
}

TEST_F(PatchRequestTest, EqualsAndNotEquals) {
  PatchRequest request(
      ProtocolVersion::ONE, {PatchFormat::BROTLI_SHARED_DICT},
      CompressedSet("data1", {{1, 10}}), CompressedSet("data2", {{2, 20}}),
      CompressedSet("data3", {{3, 30}}), CompressedSet("data4", {{4, 40}}),
      98989898L, 34343434L, 12121212L, ConnectionSpeed::VERY_FAST);

  ASSERT_EQ(request, PatchRequest(request));
  ASSERT_NE(request, PatchRequest(request).SetProtocolVersion(
                         static_cast<ProtocolVersion>(-1)));
  ASSERT_NE(request, PatchRequest(request).ResetProtocolVersion());
  ASSERT_NE(request,
            PatchRequest(request).SetAcceptFormats({PatchFormat::VCDIFF}));
  ASSERT_NE(request, PatchRequest(request).ResetAcceptFormats());
  ASSERT_NE(request, PatchRequest(request).SetCodepointsHave(
                         CompressedSet("other", {})));
  ASSERT_NE(request, PatchRequest(request).ResetCodepointsHave());
  ASSERT_NE(request, PatchRequest(request).SetCodepointsNeeded(
                         CompressedSet("other", {})));
  ASSERT_NE(request, PatchRequest(request).ResetCodepointsNeeded());
  ASSERT_NE(request,
            PatchRequest(request).SetIndicesHave(CompressedSet("other", {})));
  ASSERT_NE(request, PatchRequest(request).ResetIndicesHave());
  ASSERT_NE(request,
            PatchRequest(request).SetIndicesNeeded(CompressedSet("other", {})));
  ASSERT_NE(request, PatchRequest(request).ResetIndicesNeeded());
  ASSERT_NE(request, PatchRequest(request).SetOrderingChecksum(42));
  ASSERT_NE(request, PatchRequest(request).ResetOrderingChecksum());
  ASSERT_NE(request, PatchRequest(request).SetOriginalFontChecksum(42));
  ASSERT_NE(request, PatchRequest(request).ResetOriginalFontChecksum());
  ASSERT_NE(request, PatchRequest(request).SetBaseChecksum(42));
  ASSERT_NE(request, PatchRequest(request).ResetBaseChecksum());
  ASSERT_NE(request, PatchRequest(request).SetConnectionSpeed(
                         ConnectionSpeed::VERY_SLOW));
  ASSERT_NE(request, PatchRequest(request).ResetConnectionSpeed());
}

}  // namespace patch_subset::cbor
