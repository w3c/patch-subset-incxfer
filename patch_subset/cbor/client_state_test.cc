#include "patch_subset/cbor/client_state.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/compressed_int_list.h"

namespace patch_subset::cbor {

using std::string;
using std::vector;

class ClientStateTest : public ::testing::Test {};

TEST_F(ClientStateTest, EmptyConstructor) {
  ClientState client_state;

  EXPECT_EQ(client_state.FontId(), "");
  EXPECT_EQ(client_state.FontData(), "");
  EXPECT_EQ(client_state.Fingerprint(), 0);
  EXPECT_TRUE(client_state.CodepointRemapping().empty());
}

TEST_F(ClientStateTest, Constructor) {
  string font_id("test.ttf");
  string font_data{"ABC"};
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{1, 5, 10};

  ClientState client_state(font_id, font_data, fingerprint, remapping);

  EXPECT_EQ(client_state.FontId(), font_id);
  EXPECT_EQ(client_state.FontData(), font_data);
  EXPECT_EQ(client_state.Fingerprint(), fingerprint);
  EXPECT_EQ(client_state.CodepointRemapping(), remapping);
}

TEST_F(ClientStateTest, CopyConstructor) {
  string font_id("test.ttf");
  string font_data{"ABC"};
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{1, 5, 10};
  ClientState other(font_id, font_data, fingerprint, remapping);

  EXPECT_EQ(ClientState(other), other);
}

TEST_F(ClientStateTest, MoveConstructor) {
  string font_id(4096, 'A');
  string font_data(4096, 'B');
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{1, 5, 10};
  // Note: This constructor *does* make a copy of the buffers.
  ClientState other(font_id, font_data, fingerprint, remapping);
  auto other_id_pointer = (uint64_t)other.FontId().data();
  auto other_data_pointer = (uint64_t)other.FontData().data();

  // This should not result in the buffers being copied.
  ClientState moved = std::move(other);

  EXPECT_EQ((uint64_t)moved.FontId().data(), other_id_pointer);
  EXPECT_EQ(moved.FontId(), font_id);
  EXPECT_EQ((uint64_t)moved.FontData().data(), other_data_pointer);
  EXPECT_EQ(moved.FontData(), font_data);
  EXPECT_EQ(moved.Fingerprint(), fingerprint);
  EXPECT_EQ(moved.CodepointRemapping(), remapping);

  // Buffers were moved out.
  EXPECT_EQ(other.FontId(), "");
  EXPECT_EQ(other.FontData(), "");
}

TEST_F(ClientStateTest, Decode) {
  string font_id(4096, 'A');
  string font_data(4096, 'B');
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{};
  cbor_item_unique_ptr map = make_cbor_map(4);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString(font_id)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeBytes(font_data)));
  CborUtils::SetField(*map, 2, cbor_move(CborUtils::EncodeInt(fingerprint)));
  cbor_item_unique_ptr remapping_field = empty_cbor_ptr();
  StatusCode sc = CompressedIntList::Encode(remapping, remapping_field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 3, move_out(remapping_field));
  ClientState client_state;

  sc = ClientState::Decode(*map, client_state);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(client_state.FontId(), font_id);
  ASSERT_EQ(client_state.FontData(), font_data);
  ASSERT_EQ(client_state.Fingerprint(), fingerprint);
  ASSERT_EQ(client_state.CodepointRemapping(), remapping);
}

TEST_F(ClientStateTest, DecodeNotAMap) {
  cbor_item_unique_ptr str = make_cbor_string("err");
  ClientState client_state;

  StatusCode sc = ClientState::Decode(*str, client_state);

  ASSERT_EQ(sc, StatusCode::kInvalidArgument);
}

TEST_F(ClientStateTest, DecodeFieldsOneAndTwo) {
  string font_id("foo.ttf");
  string data("QWERTY");
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString(font_id)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeBytes(data)));
  // No field # 2 or 3.
  ClientState client_state;

  StatusCode sc = ClientState::Decode(*map, client_state);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(client_state.FontId(), font_id);
  ASSERT_EQ(client_state.FontData(), data);
  ASSERT_FALSE(client_state.HasFingerprint());
  ASSERT_EQ(client_state.Fingerprint(), 0);
  ASSERT_FALSE(client_state.HasCodepointRemapping());
  ASSERT_TRUE(client_state.CodepointRemapping().empty());
}

TEST_F(ClientStateTest, DecodeFieldsThreeAndFour) {
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{};
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 2, cbor_move(CborUtils::EncodeInt(fingerprint)));
  cbor_item_unique_ptr remapping_field = empty_cbor_ptr();
  StatusCode sc = CompressedIntList::Encode(remapping, remapping_field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 3, move_out(remapping_field));
  // No field # 0 or 1.
  ClientState client_state;

  sc = ClientState::Decode(*map, client_state);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_FALSE(client_state.HasFontId());
  ASSERT_EQ(client_state.FontId(), "");
  ASSERT_FALSE(client_state.HasFontData());
  ASSERT_EQ(client_state.FontData(), "");
  ASSERT_EQ(client_state.Fingerprint(), fingerprint);
  ASSERT_EQ(client_state.CodepointRemapping(), remapping);
}

TEST_F(ClientStateTest, Encode) {
  string font_id("foo.ttf");
  string font_data("font-data");
  uint64_t fingerprint = 999L;
  vector<int32_t> remapping{5, 10, 15, 20};
  ClientState client_state(font_id, font_data, fingerprint, remapping);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = client_state.Encode(result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 4);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*result, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  ASSERT_TRUE(cbor_isa_string(field.get()));
  ASSERT_EQ(cbor_string_length(field.get()), 7);
  unsigned char* handle = cbor_string_handle(field.get());
  ASSERT_EQ(strncmp((char*)handle, font_id.c_str(), 7), 0);

  sc = CborUtils::GetField(*result, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  string result_font_data;
  sc = CborUtils::DecodeBytes(*field, result_font_data);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_font_data, font_data);

  sc = CborUtils::GetField(*result, 2, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field, nullptr);
  uint64_t n;
  sc = CborUtils::DecodeUInt64(*field, &n);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(n, fingerprint);

  field = nullptr;
  sc = CborUtils::GetField(*result, 3, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  vector<int32_t> v;
  sc = CompressedIntList::Decode(*field, v);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(v, remapping);
}

TEST_F(ClientStateTest, EncodeFieldsTwoAndThree) {
  string font_data("font-data");
  uint64_t fingerprint = 999L;
  ClientState client_state;
  client_state.SetFontData(font_data).SetFingerprint(fingerprint);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = client_state.Encode(result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 2);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*result, 0, field);
  ASSERT_EQ(sc, StatusCode::kNotFound);

  sc = CborUtils::GetField(*result, 1, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  string result_font_data;
  sc = CborUtils::DecodeBytes(*field, result_font_data);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(result_font_data, font_data);

  sc = CborUtils::GetField(*result, 2, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field, nullptr);
  uint64_t n;
  sc = CborUtils::DecodeUInt64(*field, &n);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(n, fingerprint);

  sc = CborUtils::GetField(*result, 3, field);
  ASSERT_EQ(sc, StatusCode::kNotFound);
}

TEST_F(ClientStateTest, EncodeFieldsOneAndFour) {
  string font_id("foo.ttf");
  vector<int32_t> remapping{5, 10, 15, 20};
  ClientState client_state;
  client_state.SetFontId(font_id).SetCodepointRemapping(remapping);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = client_state.Encode(result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 2);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*result, 0, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  ASSERT_TRUE(cbor_isa_string(field.get()));
  ASSERT_EQ(cbor_string_length(field.get()), 7);
  unsigned char* handle = cbor_string_handle(field.get());
  ASSERT_EQ(strncmp((char*)handle, font_id.c_str(), 7), 0);

  sc = CborUtils::GetField(*result, 1, field);
  ASSERT_EQ(sc, StatusCode::kNotFound);

  sc = CborUtils::GetField(*result, 2, field);
  ASSERT_EQ(sc, StatusCode::kNotFound);

  field = nullptr;
  sc = CborUtils::GetField(*result, 3, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  vector<int32_t> v;
  sc = CompressedIntList::Decode(*field, v);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(v, remapping);
}

TEST_F(ClientStateTest, GettersAndSetters) {
  ClientState cs;

  // Initially empty.
  EXPECT_FALSE(cs.HasFontId());
  EXPECT_FALSE(cs.HasFontData());
  EXPECT_FALSE(cs.HasFingerprint());
  EXPECT_FALSE(cs.HasCodepointRemapping());

  // Default values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.Fingerprint(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());

  // Now set with default values.
  cs.SetFontId("");
  cs.SetFontData("");
  cs.SetFingerprint(0);
  cs.SetCodepointRemapping(vector<int32_t>{});

  // Not empty anymore.
  EXPECT_TRUE(cs.HasFontId());
  EXPECT_TRUE(cs.HasFontData());
  EXPECT_TRUE(cs.HasFingerprint());
  EXPECT_TRUE(cs.HasCodepointRemapping());

  // Double check values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.Fingerprint(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());

  // Use normal/real values.
  cs.SetFontId("font_id");
  cs.SetFontData("font_data");
  cs.SetFingerprint(12345);
  vector<int32_t> remapping{1, 5, 10, 20};
  cs.SetCodepointRemapping(remapping);

  // Still not empty.
  EXPECT_TRUE(cs.HasFontId());
  EXPECT_TRUE(cs.HasFontData());
  EXPECT_TRUE(cs.HasFingerprint());
  EXPECT_TRUE(cs.HasCodepointRemapping());

  // Double check values.
  EXPECT_EQ(cs.FontId(), "font_id");
  EXPECT_EQ(cs.FontData(), "font_data");
  EXPECT_EQ(cs.Fingerprint(), 12345);
  EXPECT_EQ(cs.CodepointRemapping(), remapping);

  // Reset fields.
  cs.ResetFontId().ResetFontData().ResetFingerprint().ResetCodepointRemapping();

  // Default values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.Fingerprint(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());
}

TEST_F(ClientStateTest, EqualsAndNotEquals) {
  ClientState cs("test.ttf", "ABC", 999L, {1, 5, 10});

  EXPECT_EQ(cs, ClientState(cs));
  EXPECT_NE(cs, ClientState(cs).SetFontId("foo"));
  EXPECT_NE(cs, ClientState(cs).ResetFontId());
  EXPECT_NE(cs, ClientState(cs).SetFontData("foo"));
  EXPECT_NE(cs, ClientState(cs).ResetFontData());
  EXPECT_NE(cs, ClientState(cs).SetFingerprint(42));
  EXPECT_NE(cs, ClientState(cs).ResetFingerprint());
  EXPECT_NE(cs, ClientState(cs).SetCodepointRemapping({4, 5, 6}));
  EXPECT_NE(cs, ClientState(cs).ResetCodepointRemapping());
}

}  // namespace patch_subset::cbor
