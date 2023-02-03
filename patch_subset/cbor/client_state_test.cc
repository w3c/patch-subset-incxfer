#include "patch_subset/cbor/client_state.h"

#include <vector>

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/cbor_utils.h"
#include "patch_subset/cbor/integer_list.h"

namespace patch_subset::cbor {

using absl::StatusCode;
using std::string;
using std::vector;

class ClientStateTest : public ::testing::Test {};

TEST_F(ClientStateTest, EmptyConstructor) {
  ClientState client_state;

  EXPECT_EQ(client_state.FontId(), "");
  EXPECT_EQ(client_state.FontData(), "");
  EXPECT_EQ(client_state.OriginalFontChecksum(), 0);
  EXPECT_TRUE(client_state.CodepointRemapping().empty());
  EXPECT_EQ(client_state.CodepointRemappingChecksum(), 0);
}

TEST_F(ClientStateTest, Constructor) {
  string font_id("test.ttf");
  string font_data{"ABC"};
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{1, 5, 10};
  uint64_t remapping_checksum = 888L;

  ClientState client_state(font_id, font_data, font_checksum, remapping,
                           remapping_checksum);

  EXPECT_EQ(client_state.FontId(), font_id);
  EXPECT_EQ(client_state.FontData(), font_data);
  EXPECT_EQ(client_state.OriginalFontChecksum(), font_checksum);
  EXPECT_EQ(client_state.CodepointRemapping(), remapping);
}

TEST_F(ClientStateTest, CopyConstructor) {
  string font_id("test.ttf");
  string font_data{"ABC"};
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{1, 5, 10};
  uint64_t remapping_checksum = 888L;
  ClientState other(font_id, font_data, font_checksum, remapping,
                    remapping_checksum);

  EXPECT_EQ(ClientState(other), other);
}

TEST_F(ClientStateTest, MoveConstructor) {
  string font_id(4096, 'A');
  string font_data(4096, 'B');
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{1, 5, 10};
  uint64_t remapping_checksum = 888L;
  // Note: This constructor *does* make a copy of the buffers.
  ClientState other(font_id, font_data, font_checksum, remapping,
                    remapping_checksum);
  auto other_id_pointer = (uint64_t)other.FontId().data();
  auto other_data_pointer = (uint64_t)other.FontData().data();

  // This should not result in the buffers being copied.
  ClientState moved = std::move(other);

  EXPECT_EQ((uint64_t)moved.FontId().data(), other_id_pointer);
  EXPECT_EQ(moved.FontId(), font_id);
  EXPECT_EQ((uint64_t)moved.FontData().data(), other_data_pointer);
  EXPECT_EQ(moved.FontData(), font_data);
  EXPECT_EQ(moved.OriginalFontChecksum(), font_checksum);
  EXPECT_EQ(moved.CodepointRemapping(), remapping);
  EXPECT_EQ(moved.CodepointRemappingChecksum(), remapping_checksum);

  // Buffers were moved out.
  EXPECT_EQ(other.FontId(), "");
  EXPECT_EQ(other.FontData(), "");
}

TEST_F(ClientStateTest, Decode) {
  string font_id(4096, 'A');
  string font_data(4096, 'B');
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{};
  uint64_t remapping_checksum = 888L;
  cbor_item_unique_ptr map = make_cbor_map(5);
  CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString(font_id)));
  CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeBytes(font_data)));
  CborUtils::SetField(*map, 2,
                      cbor_move(CborUtils::EncodeUInt64(font_checksum)));
  cbor_item_unique_ptr remapping_field = empty_cbor_ptr();
  StatusCode sc = IntegerList::Encode(remapping, remapping_field);
  ASSERT_EQ(sc, StatusCode::kOk);
  CborUtils::SetField(*map, 3, move_out(remapping_field));
  CborUtils::SetField(*map, 4,
                      cbor_move(CborUtils::EncodeUInt64(remapping_checksum)));
  ClientState client_state;

  sc = ClientState::Decode(*map, client_state);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(client_state.FontId(), font_id);
  ASSERT_EQ(client_state.FontData(), font_data);
  ASSERT_EQ(client_state.OriginalFontChecksum(), font_checksum);
  ASSERT_EQ(client_state.CodepointRemapping(), remapping);
  ASSERT_EQ(client_state.CodepointRemappingChecksum(), remapping_checksum);
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
  ASSERT_FALSE(client_state.HasOriginalFontChecksum());
  ASSERT_EQ(client_state.OriginalFontChecksum(), 0);
  ASSERT_FALSE(client_state.HasCodepointRemapping());
  ASSERT_TRUE(client_state.CodepointRemapping().empty());
  ASSERT_FALSE(client_state.HasCodepointRemappingChecksum());
}

TEST_F(ClientStateTest, DecodeFieldsThreeAndFour) {
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{};
  cbor_item_unique_ptr map = make_cbor_map(2);
  CborUtils::SetField(*map, 2,
                      cbor_move(CborUtils::EncodeUInt64(font_checksum)));
  cbor_item_unique_ptr remapping_field = empty_cbor_ptr();
  StatusCode sc = IntegerList::Encode(remapping, remapping_field);
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
  ASSERT_EQ(client_state.OriginalFontChecksum(), font_checksum);
  ASSERT_EQ(client_state.CodepointRemapping(), remapping);
  ASSERT_FALSE(client_state.HasCodepointRemappingChecksum());
}

TEST_F(ClientStateTest, Encode) {
  string font_id("foo.ttf");
  string font_data("font-data");
  uint64_t font_checksum = 999L;
  vector<int32_t> remapping{5, 10, 15, 20};
  uint64_t remapping_checksum = 888L;
  ClientState client_state(font_id, font_data, font_checksum, remapping,
                           remapping_checksum);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  StatusCode sc = client_state.Encode(result);

  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(cbor_isa_map(result.get()));
  ASSERT_EQ(cbor_map_size(result.get()), 5);

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
  ASSERT_EQ(n, font_checksum);

  field = nullptr;
  sc = CborUtils::GetField(*result, 3, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_NE(field.get(), nullptr);
  vector<int32_t> v;
  sc = IntegerList::Decode(*field, v);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(v, remapping);

  sc = CborUtils::GetField(*result, 4, field);
  ASSERT_EQ(sc, StatusCode::kOk);
  sc = CborUtils::DecodeUInt64(*field, &n);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(n, remapping_checksum);
}

TEST_F(ClientStateTest, EncodeFieldsTwoAndThree) {
  string font_data("font-data");
  uint64_t font_checksum = 999L;
  ClientState client_state;
  client_state.SetFontData(font_data).SetOriginalFontChecksum(font_checksum);
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
  ASSERT_EQ(n, font_checksum);

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
  sc = IntegerList::Decode(*field, v);
  ASSERT_EQ(sc, StatusCode::kOk);
  ASSERT_EQ(v, remapping);
}

TEST_F(ClientStateTest, GettersAndSetters) {
  ClientState cs;

  // Initially empty.
  EXPECT_FALSE(cs.HasFontId());
  EXPECT_FALSE(cs.HasFontData());
  EXPECT_FALSE(cs.HasOriginalFontChecksum());
  EXPECT_FALSE(cs.HasCodepointRemapping());
  EXPECT_FALSE(cs.HasCodepointRemappingChecksum());

  // Default values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.OriginalFontChecksum(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());
  EXPECT_EQ(cs.CodepointRemappingChecksum(), 0);

  // Now set with default values.
  cs.SetFontId("");
  cs.SetFontData("");
  cs.SetOriginalFontChecksum(0);
  cs.SetCodepointRemapping(vector<int32_t>{});
  cs.SetCodepointRemappingChecksum(0);

  // Not empty anymore.
  EXPECT_TRUE(cs.HasFontId());
  EXPECT_TRUE(cs.HasFontData());
  EXPECT_TRUE(cs.HasOriginalFontChecksum());
  EXPECT_TRUE(cs.HasCodepointRemapping());
  EXPECT_TRUE(cs.HasCodepointRemappingChecksum());

  // Double check values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.OriginalFontChecksum(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());
  EXPECT_EQ(cs.CodepointRemappingChecksum(), 0);

  // Use normal/real values.
  cs.SetFontId("font_id");
  cs.SetFontData("font_data");
  cs.SetOriginalFontChecksum(12345);
  vector<int32_t> remapping{1, 5, 10, 20};
  cs.SetCodepointRemapping(remapping);
  cs.SetCodepointRemappingChecksum(9876);

  // Still not empty.
  EXPECT_TRUE(cs.HasFontId());
  EXPECT_TRUE(cs.HasFontData());
  EXPECT_TRUE(cs.HasOriginalFontChecksum());
  EXPECT_TRUE(cs.HasCodepointRemapping());
  EXPECT_TRUE(cs.HasCodepointRemappingChecksum());

  // Double check values.
  EXPECT_EQ(cs.FontId(), "font_id");
  EXPECT_EQ(cs.FontData(), "font_data");
  EXPECT_EQ(cs.OriginalFontChecksum(), 12345);
  EXPECT_EQ(cs.CodepointRemapping(), remapping);
  EXPECT_EQ(cs.CodepointRemappingChecksum(), 9876);

  // Reset fields.
  cs.ResetFontId()
      .ResetFontData()
      .ResetOriginalFontChecksum()
      .ResetCodepointRemapping()
      .ResetCodepointRemappingChecksum();

  // Default values.
  EXPECT_EQ(cs.FontId(), "");
  EXPECT_EQ(cs.FontData(), "");
  EXPECT_EQ(cs.OriginalFontChecksum(), 0);
  EXPECT_TRUE(cs.CodepointRemapping().empty());
  EXPECT_EQ(cs.CodepointRemappingChecksum(), 0);
}

TEST_F(ClientStateTest, EqualsAndNotEquals) {
  ClientState cs("test.ttf", "ABC", 999L, {1, 5, 10}, 888L);

  EXPECT_EQ(cs, ClientState(cs));
  EXPECT_NE(cs, ClientState(cs).SetFontId("foo"));
  EXPECT_NE(cs, ClientState(cs).ResetFontId());
  EXPECT_NE(cs, ClientState(cs).SetFontData("foo"));
  EXPECT_NE(cs, ClientState(cs).ResetFontData());
  EXPECT_NE(cs, ClientState(cs).SetOriginalFontChecksum(42));
  EXPECT_NE(cs, ClientState(cs).ResetOriginalFontChecksum());
  EXPECT_NE(cs, ClientState(cs).SetCodepointRemapping({4, 5, 6}));
  EXPECT_NE(cs, ClientState(cs).ResetCodepointRemapping());
  EXPECT_NE(cs, ClientState(cs).ResetCodepointRemappingChecksum());
}

TEST_F(ClientStateTest, Serialization) {
  ClientState input("font id", "font bytes go here", 123456,
                    vector<int32_t>{1, 2, 3}, 98765);
  string serialized_bytes;
  ClientState result;

  EXPECT_EQ(input.SerializeToString(serialized_bytes), StatusCode::kOk);
  EXPECT_EQ(ClientState::ParseFromString(serialized_bytes, result),
            StatusCode::kOk);

  EXPECT_EQ(input, result);
}

TEST_F(ClientStateTest, ToString) {
  ClientState input("font id", "font bytes go here", 123456,
                    vector<int32_t>{1, 2, 3}, 98765);
  EXPECT_EQ(input.ToString(),
            "{id=font id,18 bytes,orig_cs=123456,cp_rm=[1,2,3],cprm_cs=98765}");
}

}  // namespace patch_subset::cbor
