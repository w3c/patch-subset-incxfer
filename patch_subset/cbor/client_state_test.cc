#include "patch_subset/cbor/client_state.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::Status;
using std::string;
using std::vector;

class ClientStateTest : public ::testing::Test {
 public:
  ClientStateTest() {
    space_subset.AddInterval(HB_TAG('a', 'a', 'a', 'a'), AxisInterval(10));
    space_original.AddInterval(HB_TAG('b', 'b', 'b', 'b'), AxisInterval(20));

    state = ClientState(font_checksum, ordering, space_subset, space_original);
  }

  uint64_t font_checksum = 999L;
  vector<int32_t> ordering{1, 5, 10};
  ClientState state;
  AxisSpace space_subset;
  AxisSpace space_original;
};

TEST_F(ClientStateTest, EmptyConstructor) {
  ClientState client_state;
  EXPECT_EQ(client_state.OriginalFontChecksum(), 0);
  EXPECT_TRUE(client_state.CodepointOrdering().empty());
  EXPECT_TRUE(client_state.SubsetAxisSpace().Empty());
  EXPECT_TRUE(client_state.OriginalAxisSpace().Empty());
}

TEST_F(ClientStateTest, Constructor) {
  EXPECT_EQ(state.OriginalFontChecksum(), font_checksum);
  EXPECT_EQ(state.CodepointOrdering(), ordering);
  EXPECT_EQ(state.SubsetAxisSpace(), space_subset);
  EXPECT_EQ(state.OriginalAxisSpace(), space_original);
}

TEST_F(ClientStateTest, CopyConstructor) {
  EXPECT_EQ(ClientState(state), state);
}

TEST_F(ClientStateTest, MoveConstructor) {
  // This should not result in the buffers being copied.
  ClientState moved = std::move(state);

  EXPECT_EQ(moved.OriginalFontChecksum(), font_checksum);
  EXPECT_EQ(moved.CodepointOrdering(), ordering);
  EXPECT_EQ(moved.SubsetAxisSpace(), space_subset);
  EXPECT_EQ(moved.OriginalAxisSpace(), space_original);

  EXPECT_TRUE(state.CodepointOrdering().empty());
  EXPECT_TRUE(state.SubsetAxisSpace().Empty());
  EXPECT_TRUE(state.OriginalAxisSpace().Empty());
}

TEST_F(ClientStateTest, Decode) {
  cbor_item_unique_ptr map = make_cbor_map(5);

  ASSERT_EQ(
      CborUtils::SetField(*map, ClientState::kOriginalFontChecksumFieldNumber,
                          cbor_move(CborUtils::EncodeUInt64(font_checksum))),
      absl::OkStatus());

  cbor_item_unique_ptr ordering_field = empty_cbor_ptr();
  Status sc = IntegerList::Encode(ordering, ordering_field);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(
      CborUtils::SetField(*map, ClientState::kCodepointOrderingFieldNumber,
                          move_out(ordering_field)),
      absl::OkStatus());

  cbor_item_unique_ptr subset_axis_space_field = empty_cbor_ptr();
  ASSERT_EQ(space_subset.Encode(subset_axis_space_field), absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(*map, ClientState::kSubsetAxisSpaceFieldNumber,
                                move_out(subset_axis_space_field)),
            absl::OkStatus());

  cbor_item_unique_ptr original_axis_space_field = empty_cbor_ptr();
  ASSERT_EQ(space_original.Encode(original_axis_space_field), absl::OkStatus());
  ASSERT_EQ(
      CborUtils::SetField(*map, ClientState::kOriginalAxisSpaceFieldNumber,
                          move_out(original_axis_space_field)),
      absl::OkStatus());

  ClientState decoded;
  ASSERT_EQ(ClientState::Decode(*map, decoded), absl::OkStatus());

  ASSERT_TRUE(decoded == state);
}

TEST_F(ClientStateTest, DecodeNotAMap) {
  cbor_item_unique_ptr str = make_cbor_string("err");
  ClientState client_state;

  Status sc = ClientState::Decode(*str, client_state);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(ClientStateTest, DecodeEmpty) {
  cbor_item_unique_ptr map = make_cbor_map(0);

  ClientState decoded;
  ASSERT_EQ(ClientState::Decode(*map, decoded), absl::OkStatus());

  ClientState empty;
  ASSERT_EQ(empty, decoded);
}

TEST_F(ClientStateTest, Encode) {
  cbor_item_unique_ptr result = empty_cbor_ptr();
  cbor_item_unique_ptr field = empty_cbor_ptr();

  Status sc = state.Encode(result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 4);

  sc = CborUtils::GetField(
      *result, ClientState::kOriginalFontChecksumFieldNumber, field);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field, nullptr);
  uint64_t n;
  sc = CborUtils::DecodeUInt64(*field, &n);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(n, font_checksum);

  field = nullptr;
  sc = CborUtils::GetField(*result, ClientState::kCodepointOrderingFieldNumber,
                           field);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field.get(), nullptr);
  vector<int32_t> v;
  sc = IntegerList::Decode(*field, v);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(v, ordering);

  field = nullptr;
  sc = CborUtils::GetField(*result, ClientState::kSubsetAxisSpaceFieldNumber,
                           field);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field.get(), nullptr);
  AxisSpace space;
  sc = AxisSpace::Decode(*field, space);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(space, space_subset);

  field = nullptr;
  sc = CborUtils::GetField(*result, ClientState::kOriginalAxisSpaceFieldNumber,
                           field);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field.get(), nullptr);
  sc = AxisSpace::Decode(*field, space);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(space, space_original);
}

TEST_F(ClientStateTest, GettersAndSetters) {
  ClientState cs;

  // Initially empty.
  EXPECT_FALSE(cs.HasOriginalFontChecksum());
  EXPECT_FALSE(cs.HasCodepointOrdering());
  EXPECT_FALSE(cs.HasSubsetAxisSpace());
  EXPECT_FALSE(cs.HasOriginalAxisSpace());

  // Now set with default values.
  AxisSpace empty;
  cs.SetOriginalFontChecksum(0);
  cs.SetCodepointOrdering(vector<int32_t>{});
  cs.SetSubsetAxisSpace(empty);
  cs.SetOriginalAxisSpace(empty);

  // Not empty anymore.
  EXPECT_TRUE(cs.HasOriginalFontChecksum());
  EXPECT_TRUE(cs.HasCodepointOrdering());
  EXPECT_TRUE(cs.HasSubsetAxisSpace());
  EXPECT_TRUE(cs.HasOriginalAxisSpace());

  // Use normal/real values.
  cs.SetOriginalFontChecksum(12345);
  vector<int32_t> ordering{1, 5, 10, 20};
  cs.SetCodepointOrdering(ordering);
  cs.SetSubsetAxisSpace(space_subset);
  cs.SetOriginalAxisSpace(space_original);

  // Still not empty.
  EXPECT_TRUE(cs.HasOriginalFontChecksum());
  EXPECT_TRUE(cs.HasCodepointOrdering());
  EXPECT_TRUE(cs.HasSubsetAxisSpace());
  EXPECT_TRUE(cs.HasOriginalAxisSpace());

  // Double check values.
  EXPECT_EQ(cs.OriginalFontChecksum(), 12345);
  EXPECT_EQ(cs.CodepointOrdering(), ordering);
  EXPECT_EQ(cs.SubsetAxisSpace(), space_subset);
  EXPECT_EQ(cs.OriginalAxisSpace(), space_original);

  // Reset fields.
  cs.ResetOriginalFontChecksum()
      .ResetCodepointOrdering()
      .ResetSubsetAxisSpace()
      .ResetOriginalAxisSpace();

  // Default values.
  EXPECT_EQ(cs.OriginalFontChecksum(), 0);
  EXPECT_TRUE(cs.CodepointOrdering().empty());
  EXPECT_TRUE(cs.SubsetAxisSpace().Empty());
  EXPECT_TRUE(cs.OriginalAxisSpace().Empty());
}

TEST_F(ClientStateTest, EqualsAndNotEquals) {
  EXPECT_EQ(state, ClientState(state));
  EXPECT_NE(state, ClientState(state).SetOriginalFontChecksum(42));
  EXPECT_NE(state, ClientState(state).ResetOriginalFontChecksum());
  EXPECT_NE(state, ClientState(state).SetCodepointOrdering({4, 5, 6}));
  EXPECT_NE(state, ClientState(state).ResetCodepointOrdering());
  EXPECT_NE(state, ClientState(state).SetSubsetAxisSpace(space_original));
  EXPECT_NE(state, ClientState(state).ResetSubsetAxisSpace());
  EXPECT_NE(state, ClientState(state).SetOriginalAxisSpace(space_subset));
  EXPECT_NE(state, ClientState(state).ResetOriginalAxisSpace());
}

TEST_F(ClientStateTest, Serialization) {
  string serialized_bytes;
  ClientState result;

  EXPECT_EQ(state.SerializeToString(serialized_bytes), absl::OkStatus());
  EXPECT_EQ(ClientState::ParseFromString(serialized_bytes, result),
            absl::OkStatus());

  EXPECT_EQ(state, result);
}

TEST_F(ClientStateTest, ToString) {
  EXPECT_EQ(state.ToString(),
            "{orig_cs=999,cp_rm=[1,5,10],subset_axis_space=a: [[10, 10]],"
            "original_axis_space=b: [[20, 20]]}");
}

TEST_F(ClientStateTest, FromFont) {
  std::string state_raw;
  EXPECT_EQ(state.SerializeToString(state_raw), absl::OkStatus());

  hb_blob_t* state_blob =
      hb_blob_create(state_raw.data(), state_raw.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face_builder = hb_face_builder_create();

  hb_face_builder_add_table(face_builder, HB_TAG('I', 'F', 'T', 'P'),
                            state_blob);
  hb_blob_destroy(state_blob);

  hb_blob_t* face_blob = hb_face_reference_blob(face_builder);
  hb_face_t* face = hb_face_create(face_blob, 0);
  hb_blob_destroy(face_blob);
  hb_face_destroy(face_builder);

  auto state_result = ClientState::FromFont(face);
  ASSERT_TRUE(state_result.ok()) << state_result.status();
  hb_face_destroy(face);

  EXPECT_EQ(*state_result, state);
}

TEST_F(ClientStateTest, FromFont_NoTable) {
  std::string state_raw;
  EXPECT_EQ(state.SerializeToString(state_raw), absl::OkStatus());

  hb_blob_t* state_blob =
      hb_blob_create(state_raw.data(), state_raw.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face_builder = hb_face_builder_create();

  hb_face_builder_add_table(face_builder, HB_TAG('I', 'F', 'T', 'A'),
                            state_blob);
  hb_blob_destroy(state_blob);

  hb_blob_t* face_blob = hb_face_reference_blob(face_builder);
  hb_face_t* face = hb_face_create(face_blob, 0);
  hb_blob_destroy(face_blob);
  hb_face_destroy(face_builder);

  auto state_result = ClientState::FromFont(face);
  EXPECT_TRUE(absl::IsInvalidArgument(state_result.status()))
      << state_result.status();
  hb_face_destroy(face);
}

TEST_F(ClientStateTest, FromFont_Invalid) {
  std::string state_raw = "abcdefg";
  hb_blob_t* state_blob =
      hb_blob_create(state_raw.data(), state_raw.size(),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face_builder = hb_face_builder_create();

  hb_face_builder_add_table(face_builder, HB_TAG('I', 'F', 'T', 'P'),
                            state_blob);
  hb_blob_destroy(state_blob);

  hb_blob_t* face_blob = hb_face_reference_blob(face_builder);
  hb_face_t* face = hb_face_create(face_blob, 0);
  hb_blob_destroy(face_blob);
  hb_face_destroy(face_builder);

  auto state_result = ClientState::FromFont(face);
  EXPECT_TRUE(absl::IsInvalidArgument(state_result.status()))
      << state_result.status();
  hb_face_destroy(face);
}

}  // namespace patch_subset::cbor
