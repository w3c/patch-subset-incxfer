#include "patch_subset/cbor/cbor_utils.h"

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

namespace patch_subset::cbor {

using absl::Status;
using absl::StrFormat;
using absl::string_view;
using std::optional;
using std::set;
using std::string;

class CborUtilsTest : public ::testing::Test {};

size_t serialized_size(int n);
bool string_transcoded(const string& s);
string bytes(const cbor_item_t& item);

TEST_F(CborUtilsTest, SetField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr value = make_cbor_int(42);

  Status sc = CborUtils::SetField(*map, 21, value.get());

  ASSERT_EQ(sc, absl::OkStatus());
  set<uint64_t> expected{21};
  ASSERT_EQ(CborUtils::MapKeys(*map), expected);
  ASSERT_EQ(cbor_map_size(map.get()), 1);
  cbor_pair* pairs = cbor_map_handle(map.get());
  ASSERT_EQ(pairs[0].value, value.get());
  ASSERT_EQ(cbor_get_int(pairs[0].key), 21);
}

TEST_F(CborUtilsTest, SetFieldNegId) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr value = make_cbor_int(42);

  Status sc = CborUtils::SetField(*map, -1, value.get());

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(CborUtilsTest, SetFieldFullMap) {
  cbor_item_unique_ptr map = make_cbor_map(0);  // No room for fields.
  cbor_item_unique_ptr value = make_cbor_int(42);

  Status sc = CborUtils::SetField(*map, 0, value.get());

  ASSERT_TRUE(absl::IsInternal(sc));
}

TEST_F(CborUtilsTest, SetFieldIndefinateMap) {
  cbor_item_unique_ptr map = wrap_cbor_item(cbor_new_indefinite_map());
  cbor_item_unique_ptr value = make_cbor_int(42);

  Status sc = CborUtils::SetField(*map, 0, value.get());

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
}

TEST_F(CborUtilsTest, SetUInt64FieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<uint64_t> n(54321);

  Status sc = CborUtils::SetUInt64Field(*map, 0, n);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  uint64_t result;
  sc = CborUtils::DecodeUInt64(*field, &result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, 54321);
}

TEST_F(CborUtilsTest, SetUInt64FieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<uint64_t> n;

  Status sc = CborUtils::SetUInt64Field(*map, 0, n);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CborUtilsTest, SetFloatFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<float> n(543.21f);

  Status sc = CborUtils::SetFloatField(*map, 0, n);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  float result;
  sc = CborUtils::DecodeFloat(*field, &result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, 543.21f);
}

TEST_F(CborUtilsTest, SetFloatFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<float> n;

  Status sc = CborUtils::SetFloatField(*map, 0, n);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CborUtilsTest, SetStringFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<string> s("foo");

  Status sc = CborUtils::SetStringField(*map, 0, s);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  string result;
  sc = CborUtils::DecodeString(*field, result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, "foo");
}

TEST_F(CborUtilsTest, SetStringFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<string> s;

  Status sc = CborUtils::SetStringField(*map, 0, s);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CborUtilsTest, SetBytesFieldPresent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<string> b("bytes-go-here");

  Status sc = CborUtils::SetBytesField(*map, 0, b);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 1);

  cbor_item_unique_ptr field = empty_cbor_ptr();
  sc = CborUtils::GetField(*map, 0, field);
  ASSERT_EQ(sc, absl::OkStatus());
  string result;
  sc = CborUtils::DecodeBytes(*field, result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, "bytes-go-here");
}

TEST_F(CborUtilsTest, SetBytesFieldAbsent) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  optional<string> b;

  Status sc = CborUtils::SetBytesField(*map, 0, b);
  ASSERT_EQ(sc, absl::OkStatus());
  EXPECT_EQ(cbor_map_size(map.get()), 0);
}

TEST_F(CborUtilsTest, GetFieldRefCount) {
  cbor_item_t* raw_key = cbor_build_uint8(0);
  ASSERT_EQ(raw_key->refcount, 1);  // Default value. "We" own it.
  cbor_item_t* raw_value = cbor_build_uint8(42);
  ASSERT_EQ(raw_value->refcount, 1);  // Default value. "We" own it.
  cbor_item_t* raw_map = cbor_new_definite_map(1);
  ASSERT_EQ(raw_map->refcount, 1);  // Default value. "We" own it.
  ASSERT_TRUE(cbor_map_add(raw_map, cbor_pair{.key = cbor_move(raw_key),
                                              .value = cbor_move(raw_value)}));

  // Use of cbor_move() decremented ref count, adding to map incremented.
  ASSERT_EQ(raw_map->refcount, 1);    // No change.
  ASSERT_EQ(raw_key->refcount, 1);    // Map owns it now.
  ASSERT_EQ(raw_value->refcount, 1);  // Map owns it now.

  // Look up the field.
  cbor_item_unique_ptr field = empty_cbor_ptr();
  Status sc = CborUtils::GetField(*raw_map, 0, field);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field.get(), nullptr);
  ASSERT_EQ((void*)field.get(), (void*)raw_value);

  ASSERT_EQ(raw_map->refcount, 1);    // No change.
  ASSERT_EQ(raw_key->refcount, 1);    // Map owns it now.
  ASSERT_EQ(raw_value->refcount, 2);  // Map and our ptr own it now.

  // We are done with the map, but still using the field we got.
  cbor_decref(&raw_map);
  ASSERT_EQ(raw_map, nullptr);
  // raw_key has been freed.
  ASSERT_EQ(raw_value->refcount, 1);  // Only our ptr owns it now.

  //  field unique pointer will free raw_value.
}

TEST_F(CborUtilsTest, GetField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr key = make_cbor_int(21);
  cbor_item_unique_ptr value = make_cbor_int(42);
  ASSERT_TRUE(cbor_map_add(map.get(),
                           cbor_pair{.key = key.get(), .value = value.get()}));
  cbor_item_unique_ptr field = empty_cbor_ptr();

  Status sc = CborUtils::GetField(*map, 21, field);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_NE(field, nullptr);
  ASSERT_EQ(cbor_get_int(field.get()), 42);
}

TEST_F(CborUtilsTest, GetFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr key = make_cbor_int(21);
  cbor_item_unique_ptr value = make_cbor_int(42);
  ASSERT_TRUE(cbor_map_add(map.get(),
                           cbor_pair{.key = key.get(), .value = value.get()}));
  cbor_item_unique_ptr field = empty_cbor_ptr();

  Status sc = CborUtils::GetField(*map, 999, field);

  ASSERT_TRUE(absl::IsNotFound(sc));
  ASSERT_EQ(field.get(), nullptr);
}

TEST_F(CborUtilsTest, GetFieldSkipInvalidEntries) {
  cbor_item_unique_ptr map = make_cbor_map(2);
  ASSERT_TRUE(cbor_map_add(
      map.get(),
      cbor_pair{.key = cbor_move(CborUtils::EncodeString("bad-key")),
                .value = cbor_move(CborUtils::EncodeString("value1"))}));
  ASSERT_TRUE(cbor_map_add(
      map.get(),
      cbor_pair{.key = cbor_move(CborUtils::EncodeInt(42)),
                .value = cbor_move(CborUtils::EncodeString("value2"))}));
  cbor_item_unique_ptr field = empty_cbor_ptr();

  Status sc = CborUtils::GetField(*map, 42, field);

  ASSERT_EQ(sc, absl::OkStatus());
  string result;
  sc = CborUtils::DecodeString(*field, result);
  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, "value2");
}

TEST_F(CborUtilsTest, GetFieldNotFoundFieldCleared) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  cbor_item_unique_ptr field = make_cbor_int(42);

  Status sc = CborUtils::GetField(*map, 999, field);

  ASSERT_TRUE(absl::IsNotFound(sc));
  ASSERT_EQ(field.get(), nullptr);  // Field cleared out.
}

TEST_F(CborUtilsTest, GetFieldNegId) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  cbor_item_unique_ptr key = make_cbor_int(21);
  cbor_item_unique_ptr value = make_cbor_int(42);
  ASSERT_TRUE(cbor_map_add(map.get(),
                           cbor_pair{.key = key.get(), .value = value.get()}));
  cbor_item_unique_ptr field = empty_cbor_ptr();

  Status sc = CborUtils::GetField(*map, -1, field);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(field.get(), nullptr);
}

TEST_F(CborUtilsTest, GetUInt64Field) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(CborUtils::SetField(*map, 0,
                                cbor_move(CborUtils::EncodeUInt64(UINT64_MAX))),
            absl::OkStatus());
  optional<uint64_t> result;

  Status sc = CborUtils::GetUInt64Field(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, UINT64_MAX);
}

TEST_F(CborUtilsTest, GetUInt64FieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<uint64_t> result(-1);

  Status sc = CborUtils::GetUInt64Field(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(CborUtilsTest, GetUInt64FieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("bad"))),
      absl::OkStatus());
  optional<uint64_t> result(-1);

  Status sc = CborUtils::GetUInt64Field(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, -1);
}

TEST_F(CborUtilsTest, GetFloatField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeFloat(1234.56f))),
      absl::OkStatus());
  optional<float> result;

  Status sc = CborUtils::GetFloatField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(*result, 1234.56f);
}

TEST_F(CborUtilsTest, GetFloatFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<float> result(-1);

  Status sc = CborUtils::GetFloatField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(CborUtilsTest, GetFloatFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("bad"))),
      absl::OkStatus());
  optional<float> result(-1);

  Status sc = CborUtils::GetFloatField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, -1);
}

TEST_F(CborUtilsTest, GetStringField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeString("foo"))),
      absl::OkStatus());
  optional<string> result;

  Status sc = CborUtils::GetStringField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, "foo");
}

TEST_F(CborUtilsTest, GetStringFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<string> result("old-value");

  Status sc = CborUtils::GetStringField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(CborUtilsTest, GetStringFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeInt(0))),
            absl::OkStatus());
  optional<string> result("old-value");

  Status sc = CborUtils::GetStringField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, "old-value");
}

TEST_F(CborUtilsTest, GetBytesField) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeBytes("foo"))),
      absl::OkStatus());
  optional<string> result;

  Status sc = CborUtils::GetBytesField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result, "foo");
}

TEST_F(CborUtilsTest, GetBytesFieldNotFound) {
  cbor_item_unique_ptr map = make_cbor_map(0);
  optional<string> result("old-value");

  Status sc = CborUtils::GetBytesField(*map, 0, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_FALSE(result.has_value());
}

TEST_F(CborUtilsTest, GetBytesFieldInvalid) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(CborUtils::SetField(*map, 0, cbor_move(CborUtils::EncodeInt(0))),
            absl::OkStatus());
  optional<string> result("old-value");

  Status sc = CborUtils::GetBytesField(*map, 0, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, "old-value");
}

TEST_F(CborUtilsTest, DecodeIntErrors) {
  EXPECT_TRUE(absl::IsInvalidArgument(
      CborUtils::DecodeInt(*make_cbor_int(0), nullptr)));
  EXPECT_TRUE(absl::IsInvalidArgument(
      CborUtils::DecodeInt(*make_cbor_string("foo"), nullptr)));
}

TEST_F(CborUtilsTest, IntDoubleConversions) {
  for (int i = -65536; i < 65536; i += 10) {
    int32_t j;
    Status sc = CborUtils::DecodeInt(*make_cbor_int(i), &j);
    ASSERT_EQ(sc, absl::OkStatus());
    ASSERT_EQ(i, j);
  }
}

TEST_F(CborUtilsTest, IntSerializedSizes) {
  ASSERT_EQ(serialized_size(0), 1);
  ASSERT_EQ(serialized_size(23), 1);
  ASSERT_EQ(serialized_size(24), 2);
  ASSERT_EQ(serialized_size(255), 2);
  ASSERT_EQ(serialized_size(256), 3);
  ASSERT_EQ(serialized_size(65535), 3);
  ASSERT_EQ(serialized_size(65536), 5);
  ASSERT_EQ(serialized_size(-1), 1);
  ASSERT_EQ(serialized_size(-22), 1);
  ASSERT_EQ(serialized_size(-23), 1);
  ASSERT_EQ(serialized_size(-24), 1);
  ASSERT_EQ(serialized_size(-25), 2);
  ASSERT_EQ(serialized_size(-26), 2);
  ASSERT_EQ(serialized_size(-255), 2);
  ASSERT_EQ(serialized_size(-256), 2);
  ASSERT_EQ(serialized_size(-257), 3);
  ASSERT_EQ(serialized_size(-65535), 3);
  ASSERT_EQ(serialized_size(-65536), 3);
  ASSERT_EQ(serialized_size(-65537), 5);
}

TEST_F(CborUtilsTest, StringEncode) {
  string s("this is a test!");

  cbor_item_unique_ptr item = wrap_cbor_item(CborUtils::EncodeString(s));

  ASSERT_TRUE(cbor_isa_string(item.get()));
  ASSERT_TRUE(cbor_string_is_definite(item.get()));
  ASSERT_EQ(cbor_string_length(item.get()), 15);
  unsigned char* handle = cbor_string_handle(item.get());
  ASSERT_EQ(strncmp((char*)handle, s.c_str(), s.size()), 0);
}

TEST_F(CborUtilsTest, DecodeString) {
  cbor_item_unique_ptr item = make_cbor_string("testing");
  string s;

  Status sc = CborUtils::DecodeString(*item, s);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(s, "testing");
}

TEST_F(CborUtilsTest, StringsExamples) {
  ASSERT_TRUE(string_transcoded(""));
  ASSERT_TRUE(string_transcoded("x"));
  ASSERT_TRUE(string_transcoded("this is a test"));
}

TEST_F(CborUtilsTest, DecodeStringNotDefinate) {
  cbor_item_unique_ptr item = wrap_cbor_item(cbor_new_indefinite_string());
  ASSERT_TRUE(cbor_string_add_chunk(item.get(),
                                    cbor_move(CborUtils::EncodeString("foo"))));
  string s = "untouched";

  Status sc = CborUtils::DecodeString(*item, s);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(s, "untouched");
}

TEST_F(CborUtilsTest, EncodeBytes) {
  string_view input{"ABC"};

  cbor_item_unique_ptr result = wrap_cbor_item(CborUtils::EncodeBytes(input));

  ASSERT_TRUE(cbor_isa_bytestring(result.get()));
  ASSERT_EQ(cbor_bytestring_length(result.get()), 3);
  unsigned char* data = cbor_bytestring_handle(result.get());
  ASSERT_EQ(data[0], 'A');
  ASSERT_EQ(data[1], 'B');
  ASSERT_EQ(data[2], 'C');
}

TEST_F(CborUtilsTest, DecodeBytes) {
  char bytes[] = {65, 66, 67};
  string_view sv{bytes, 3};
  cbor_item_unique_ptr item = make_cbor_bytestring(sv);
  string result;

  Status sc = CborUtils::DecodeBytes(*item, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(result.size(), 3);
  ASSERT_EQ(result, "ABC");
}

TEST_F(CborUtilsTest, DecodeBytesEmpty) {
  cbor_item_unique_ptr item = make_cbor_bytestring("");
  string result;

  Status sc = CborUtils::DecodeBytes(*item, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_TRUE(result.empty());
}

TEST_F(CborUtilsTest, BytesFromString) {
  auto input_string = std::make_unique<string>("this is a test");
  string_view input(*input_string);
  string output;

  // Mutate and free the memory inputs, to ensure a copy is made.
  cbor_item_unique_ptr encoded = wrap_cbor_item(CborUtils::EncodeBytes(input));
  input_string.get()[0] = 'X';
  input_string.reset(nullptr);
  Status sc = CborUtils::DecodeBytes(*encoded, output);
  encoded.get()->data[0] = 'Y';
  encoded.reset(nullptr);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(output, "this is a test");
}

TEST_F(CborUtilsTest, DecodeBytesNotDefinate) {
  cbor_item_unique_ptr indef = wrap_cbor_item(cbor_new_indefinite_bytestring());
  ASSERT_TRUE(cbor_bytestring_add_chunk(
      indef.get(), cbor_move(CborUtils::EncodeBytes("foo"))));
  string result = "untouched";

  Status sc = CborUtils::DecodeBytes(*indef, result);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(result, "untouched");
}

TEST_F(CborUtilsTest, EncodeUInt64) {
  EXPECT_EQ(
      cbor_int_get_width(wrap_cbor_item(CborUtils::EncodeUInt64(0)).get()),
      CBOR_INT_8);
  EXPECT_EQ(
      cbor_int_get_width(wrap_cbor_item(CborUtils::EncodeUInt64(255)).get()),
      CBOR_INT_8);
  EXPECT_EQ(
      cbor_int_get_width(wrap_cbor_item(CborUtils::EncodeUInt64(256)).get()),
      CBOR_INT_16);
  EXPECT_EQ(
      cbor_int_get_width(wrap_cbor_item(CborUtils::EncodeUInt64(65535)).get()),
      CBOR_INT_16);
  EXPECT_EQ(
      cbor_int_get_width(wrap_cbor_item(CborUtils::EncodeUInt64(65536)).get()),
      CBOR_INT_32);
  EXPECT_EQ(cbor_int_get_width(
                wrap_cbor_item(CborUtils::EncodeUInt64(UINT64_MAX)).get()),
            CBOR_INT_64);
}

TEST_F(CborUtilsTest, SerializeToBytes) {
  unsigned char buffer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  unsigned char expected[] = {0x63, 0x41, 0x42, 0x43, 0xFF, 0xFF};
  string_view sv((char*)buffer, 6);
  cbor_item_unique_ptr n = make_cbor_string("ABC");
  size_t num_bytes = -1;

  Status sc = CborUtils::SerializeToBytes(*n, sv, &num_bytes);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(num_bytes, 4);
  ASSERT_EQ(strncmp((char*)buffer, (char*)expected, 6), 0);
}

TEST_F(CborUtilsTest, SerializeToBytesBufferTooSmall) {
  unsigned char buffer[] = {0xFF, 0xFF};
  string_view sv((char*)buffer, 2);
  cbor_item_unique_ptr n = make_cbor_string("ABC");
  size_t num_bytes = -1;

  Status sc = CborUtils::SerializeToBytes(*n, sv, &num_bytes);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(num_bytes, -1);
}

TEST_F(CborUtilsTest, SerializeToBytesEmptyBuffer) {
  string_view sv;
  cbor_item_unique_ptr n = make_cbor_string("ABC");
  size_t num_bytes = -1;

  Status sc = CborUtils::SerializeToBytes(*n, sv, &num_bytes);

  ASSERT_TRUE(absl::IsInvalidArgument(sc));
  ASSERT_EQ(num_bytes, -1);
}

TEST_F(CborUtilsTest, SerializeToBytesExamples) {
  ASSERT_EQ(bytes(*make_cbor_int(0)), "00");
  ASSERT_EQ(bytes(*make_cbor_int(1)), "01");
  ASSERT_EQ(bytes(*make_cbor_int(2)), "02");
  ASSERT_EQ(bytes(*make_cbor_int(0xdead)), "19 de ad");

  ASSERT_EQ(bytes(*make_cbor_string("")), "60");
  ASSERT_EQ(bytes(*make_cbor_string("A")), "61 41");
  ASSERT_EQ(bytes(*make_cbor_string("AB")), "62 41 42");
  ASSERT_EQ(bytes(*make_cbor_string("ABC")), "63 41 42 43");

  unsigned char buf[]{0, 127, 255, 127, 0};
  ASSERT_EQ(bytes(*make_cbor_bytestring({(char*)buf, 1})), "41 00");
  ASSERT_EQ(bytes(*make_cbor_bytestring({(char*)buf, 3})), "43 00 7f ff");
  ASSERT_EQ(bytes(*make_cbor_bytestring({(char*)buf, 5})), "45 00 7f ff 7f 00");

  ASSERT_EQ(bytes(*make_cbor_map(0)), "a0");

  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(CborUtils::SetField(*map, 7, cbor_move(CborUtils::EncodeInt(9))),
            absl::OkStatus());
  ASSERT_EQ(bytes(*map), "a1 07 09");

  map = make_cbor_map(2);
  ASSERT_EQ(
      CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeString("ABC"))),
      absl::OkStatus());
  ASSERT_EQ(CborUtils::SetField(
                *map, 2, cbor_move(CborUtils::EncodeBytes({(char*)buf, 5}))),
            absl::OkStatus());
  ASSERT_EQ(bytes(*map), "a2 01 63 41 42 43 02 45 00 7f ff 7f 00");
}

TEST_F(CborUtilsTest, DeserializeFromBytes) {
  cbor_item_unique_ptr map = make_cbor_map(1);
  ASSERT_EQ(
      CborUtils::SetField(*map, 1, cbor_move(CborUtils::EncodeString("foo"))),
      absl::OkStatus());
  const size_t len = 16;
  char buffer[len];
  string_view sv(buffer, len);
  size_t num_bytes;
  Status sc = CborUtils::SerializeToBytes(*map, sv, &num_bytes);
  ASSERT_EQ(sc, absl::OkStatus());
  string_view serialized_bytes = string_view(buffer, num_bytes);
  cbor_item_unique_ptr result = empty_cbor_ptr();

  sc = CborUtils::DeserializeFromBytes(serialized_bytes, result);

  ASSERT_EQ(sc, absl::OkStatus());
  ASSERT_EQ(CborUtils::MapKeys(*result), set<uint64_t>{1});
  optional<string> field;
  ASSERT_EQ(CborUtils::GetStringField(*result, 1, field), absl::OkStatus());
  ASSERT_EQ(field, "foo");
}

size_t serialized_size(int n) {
  const size_t buffer_size = 8;
  unsigned char buffer[buffer_size];
  cbor_item_unique_ptr cbor_int = make_cbor_int(n);
  size_t bytes_used = cbor_serialize(cbor_int.get(), buffer, buffer_size);
  return bytes_used;
}

bool string_transcoded(const string& s) {
  cbor_item_unique_ptr item = wrap_cbor_item(CborUtils::EncodeString(s));
  string s2;
  if (CborUtils::DecodeString(*item, s2) != absl::OkStatus()) {
    return false;
  }
  bool matched = (s == s2);
  return matched;
}

string bytes(const cbor_item_t& item) {
  const size_t len = 1024;
  char buffer[len] = {0};
  string_view sv(buffer, len);
  size_t num_bytes = 0;
  if (CborUtils::SerializeToBytes(item, sv, &num_bytes) != absl::OkStatus()) {
    return "Serialization error!";
  }
  string s;
  for (size_t i = 0; i < num_bytes; i++) {
    s += StrFormat("%02x", buffer[i]);
    if (i < num_bytes - 1) {
      s += " ";
    }
  }
  return s;
}

}  // namespace patch_subset::cbor
