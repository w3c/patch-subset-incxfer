#include "patch_subset/cbor/patch_request.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

class PatchRequestTest : public ::testing::Test {};

using absl::Status;
using std::string;
using std::vector;

TEST_F(PatchRequestTest, EmptyConstructor) {
  PatchRequest request;

  EXPECT_TRUE(request.CodepointsHave().empty());
  EXPECT_TRUE(request.CodepointsNeeded().empty());
  EXPECT_TRUE(request.IndicesHave().empty());
  EXPECT_TRUE(request.IndicesNeeded().empty());
  EXPECT_EQ(request.OrderingChecksum(), 0);
  EXPECT_EQ(request.OriginalFontChecksum(), 0);
  EXPECT_EQ(request.BaseChecksum(), 0);
}

TEST_F(PatchRequestTest, Constructor) {
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 98989898L;
  uint64_t original_font_checksum = 34343434L;
  uint64_t base_checksum = 12121212L;

  PatchRequest request(codepoints_have,
                       codepoints_needed, indices_have, indices_needed,
                       ordering_checksum, original_font_checksum, base_checksum);

  EXPECT_EQ(request.CodepointsHave(), codepoints_have);
  EXPECT_EQ(request.CodepointsNeeded(), codepoints_needed);
  EXPECT_EQ(request.IndicesHave(), indices_have);
  EXPECT_EQ(request.IndicesNeeded(), indices_needed);
  EXPECT_EQ(request.OrderingChecksum(), ordering_checksum);
  EXPECT_EQ(request.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(request.BaseChecksum(), base_checksum);

  EXPECT_EQ(PatchRequest(request), request);
}

TEST_F(PatchRequestTest, CopyConstructor) {
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 98989898L;
  uint64_t original_font_checksum = 34343434L;
  uint64_t base_checksum = 12121212L;
  PatchRequest request(codepoints_have,
                       codepoints_needed, indices_have, indices_needed,
                       ordering_checksum, original_font_checksum, base_checksum);

  EXPECT_EQ(PatchRequest(request), request);
}

TEST_F(PatchRequestTest, MoveConstructor) {
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 98989898L;
  uint64_t original_font_checksum = 34343434L;
  uint64_t base_checksum = 12121212L;

  PatchRequest origional(codepoints_have,
                         codepoints_needed, indices_have, indices_needed,
                         ordering_checksum, original_font_checksum,
                         base_checksum);
  PatchRequest copy(origional);

  PatchRequest moved = std::move(origional);

  EXPECT_EQ(moved, copy);
}

TEST_F(PatchRequestTest, Decode) {
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 4567;
  uint64_t original_font_checksum = 5678;
  uint64_t base_checksum = 6789;

  PatchRequest expected(codepoints_have,
                        codepoints_needed, indices_have, indices_needed,
                        ordering_checksum, original_font_checksum,
                        base_checksum);

  cbor_item_unique_ptr map = make_cbor_map(10);
  cbor_item_unique_ptr field = empty_cbor_ptr();

  ASSERT_EQ(codepoints_have.Encode(field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, PatchRequest::kCodepointsHaveFieldNumber,
                                move_out(field)),
            absl::OkStatus());

  ASSERT_EQ(codepoints_needed.Encode(field), absl::OkStatus());
  ASSERT_EQ(
      CborUtils::SetField(*map, PatchRequest::kCodepointsNeededFieldNumber,
                          move_out(field)),
      absl::OkStatus());

  ASSERT_EQ(indices_have.Encode(field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, PatchRequest::kIndicesHaveFieldNumber,
                                move_out(field)),
            absl::OkStatus());

  ASSERT_EQ(indices_needed.Encode(field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, PatchRequest::kIndicesNeededFieldNumber,
                                move_out(field)),
            absl::OkStatus());

  ASSERT_EQ(CborUtils::SetField(
                *map, PatchRequest::kOrderingChecksumFieldNumber,
                cbor_move(CborUtils::EncodeUInt64(ordering_checksum))),
            absl::OkStatus());

  ASSERT_EQ(CborUtils::SetField(
                *map, PatchRequest::kOriginalFontChecksumFieldNumber,
                cbor_move(CborUtils::EncodeUInt64(original_font_checksum))),
            absl::OkStatus());

  ASSERT_EQ(
      CborUtils::SetField(*map, PatchRequest::kBaseChecksumFieldNumber,
                          cbor_move(CborUtils::EncodeUInt64(base_checksum))),
      absl::OkStatus());

  PatchRequest result;

  Status sc = PatchRequest::Decode(*map, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, expected);
}

TEST_F(PatchRequestTest, Encode) {
  CompressedSet codepoints_have(string{"data1"}, range_vector{{1, 10}});
  CompressedSet codepoints_needed(string{"data2"}, range_vector{{2, 20}});
  CompressedSet indices_have(string{"data3"}, range_vector{});
  CompressedSet indices_needed(string{"data4"}, range_vector{{3, 30}});
  uint64_t ordering_checksum = 34343434L;
  uint64_t original_font_checksum = 12121212L;
  uint64_t base_checksum = 98989898L;

  PatchRequest request(codepoints_have,
                       codepoints_needed, indices_have, indices_needed,
                       ordering_checksum, original_font_checksum, base_checksum);
  cbor_item_unique_ptr map = empty_cbor_ptr();

  Status sc = request.Encode(map);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_TRUE(cbor_isa_map(map.get()));
  ASSERT_EQ(cbor_map_size(map.get()), 10);
  cbor_item_unique_ptr field = empty_cbor_ptr();

  sc = CborUtils::GetField(*map, PatchRequest::kCodepointsHaveFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  CompressedSet result_codepoints_have;
  sc = CompressedSet::Decode(*field, result_codepoints_have);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_codepoints_have, codepoints_have);

  sc = CborUtils::GetField(*map, PatchRequest::kCodepointsNeededFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  CompressedSet result_codepoints_needed;
  sc = CompressedSet::Decode(*field, result_codepoints_needed);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_codepoints_needed, codepoints_needed);

  sc = CborUtils::GetField(*map, PatchRequest::kIndicesHaveFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  CompressedSet result_indices_have;
  sc = CompressedSet::Decode(*field, result_indices_have);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_indices_have, indices_have);

  sc = CborUtils::GetField(*map, PatchRequest::kIndicesNeededFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  CompressedSet result_indices_needed;
  sc = CompressedSet::Decode(*field, result_indices_needed);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_indices_needed, indices_needed);

  sc = CborUtils::GetField(*map, PatchRequest::kOrderingChecksumFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  uint64_t result_ordering_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_ordering_checksum);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_ordering_checksum, ordering_checksum);

  sc = CborUtils::GetField(*map, PatchRequest::kOriginalFontChecksumFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  uint64_t result_original_font_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_original_font_checksum);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_original_font_checksum, original_font_checksum);

  sc = CborUtils::GetField(*map, PatchRequest::kBaseChecksumFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  uint64_t result_base_checksum;
  sc = CborUtils::DecodeUInt64(*field, &result_base_checksum);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result_base_checksum, base_checksum);
}

TEST_F(PatchRequestTest, GettersAndSetters) {
  PatchRequest request;

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
}

TEST_F(PatchRequestTest, EqualsAndNotEquals) {
  PatchRequest request(
      CompressedSet("data1", {{1, 10}}), CompressedSet("data2", {{2, 20}}),
      CompressedSet("data3", {{3, 30}}), CompressedSet("data4", {{4, 40}}),
      98989898L, 34343434L, 12121212L);

  ASSERT_EQ(request, PatchRequest(request));
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
}

TEST_F(PatchRequestTest, Serialization) {
  PatchRequest input(
      CompressedSet{"bit-set-bytes1", range_vector{{1, 10}}},
      CompressedSet{"bit-set-bytes2", range_vector{{11, 12}}},
      CompressedSet{"bit-set-bytes3", range_vector{{5, 6}}},
      CompressedSet{"bit-set-bytes4", range_vector{{7, 8}}}, 12345L, 23456L,
      34567L);
  string serialized_bytes;
  PatchRequest result;

  EXPECT_EQ(input.SerializeToString(serialized_bytes), absl::OkStatus());
  EXPECT_EQ(PatchRequest::ParseFromString(serialized_bytes, result),
            absl::OkStatus());

  EXPECT_EQ(input, result);
}

TEST_F(PatchRequestTest, ToString) {
  PatchRequest input(
      CompressedSet{"bit-set-bytes1", range_vector{{1, 10}}},
      CompressedSet{"", range_vector{{11, 12}}},
      CompressedSet{"", range_vector{{5, 6}}},
      CompressedSet{"", range_vector{{7, 8}}}, 12345L, 23456L, 34567L);
  ASSERT_EQ(input.ToString(),
            "{cp_have={[1-10],bitset=14b},"
            "cp_need={[11-12]},i_have={[5-6]},"
            "i_need={[7-8]},orig_cs=23456,ord_cs=12345,"
            "base_cs=34567}");
}

}  // namespace patch_subset::cbor
