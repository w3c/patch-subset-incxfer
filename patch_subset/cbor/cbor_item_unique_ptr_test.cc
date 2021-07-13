#include "patch_subset/cbor/cbor_item_unique_ptr.h"

#include "gtest/gtest.h"
#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

class CborItemUniquePtrTest : public ::testing::Test {};

TEST_F(CborItemUniquePtrTest, Empty) {
  cbor_item_unique_ptr empty = empty_cbor_ptr();

  EXPECT_EQ(empty.get(), nullptr);
}

TEST_F(CborItemUniquePtrTest, WrapCborItem) {
  cbor_item_t* item = cbor_new_definite_map(16);

  cbor_item_unique_ptr ptr = wrap_cbor_item(item);

  EXPECT_EQ(ptr.get(), item);
  EXPECT_EQ((uint64_t)ptr.get(), (uint64_t)item);

  // Map (and internal memory) will be freed automatically.
}

TEST_F(CborItemUniquePtrTest, WrapCborItemNullAllowed) {
  cbor_item_unique_ptr ptr = wrap_cbor_item(nullptr);

  EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(CborItemUniquePtrTest, MakeCborMap) {
  cbor_item_unique_ptr map = make_cbor_map(4);

  EXPECT_TRUE(cbor_isa_map(map.get()));

  // Map will be deleted automatically.
}

TEST_F(CborItemUniquePtrTest, MakeCborArray) {
  cbor_item_unique_ptr array = make_cbor_array(12);

  EXPECT_TRUE(cbor_isa_array(array.get()));

  // Array will be deleted automatically.
}

TEST_F(CborItemUniquePtrTest, MakeCborInt) {
  cbor_item_unique_ptr cint = make_cbor_int(1234);
  int32_t n;

  StatusCode sc = CborUtils::DecodeInt(*cint, &n);

  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(n, 1234);

  // Int will be deleted automatically.
}

TEST_F(CborItemUniquePtrTest, MakeCborString) {
  cbor_item_unique_ptr str = make_cbor_string("abc");
  string s;

  StatusCode sc = CborUtils::DecodeString(*str, s);

  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(s, "abc");

  // String will be deleted automatically.
}

TEST_F(CborItemUniquePtrTest, MakeCborBytestring) {
  string buffer("data bytes go here");
  cbor_item_unique_ptr bytes = make_cbor_bytestring(buffer);

  string sv;
  StatusCode sc = CborUtils::DecodeBytes(*bytes, sv);

  ASSERT_EQ(sc, StatusCode::kOk);
  EXPECT_EQ(sv, buffer);

  // Bytestring will be deleted automatically.
}

TEST_F(CborItemUniquePtrTest, MoveOut) {
  cbor_item_t* item = cbor_build_uint8(0);
  ASSERT_EQ(item->refcount, 1);  // We are holding it.
  cbor_item_unique_ptr ptr = wrap_cbor_item(item);
  ASSERT_EQ(ptr.get(), item);
  ASSERT_EQ(item->refcount, 1);  // No change.

  cbor_item_t* about_to_put_in_map = move_out(ptr);

  ASSERT_EQ(ptr.get(), nullptr);         // No longer managed.
  ASSERT_EQ(about_to_put_in_map, item);  // Got correct pointer out.
  ASSERT_EQ(item->refcount, 0);          // We are not owning it.
  // Pretend we are a container taking ownership of the item;
  cbor_incref(item);
  ASSERT_EQ(item->refcount, 1);

  // Item is no longer managed, so we must free it.
  cbor_decref(&item);
  ASSERT_EQ(item, nullptr);
}

}  // namespace patch_subset::cbor
