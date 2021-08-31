#include "patch_subset/cbor/cbor_item_unique_ptr.h"

#include <memory>

#include "patch_subset/cbor/cbor_utils.h"

namespace patch_subset::cbor {

using absl::string_view;

void delete_cbor_item(cbor_item_t* item) {
  // Note: cbor_decref will set only this local copy of the pointer to NULL.
  cbor_decref(&item);
}

cbor_item_unique_ptr empty_cbor_ptr() {
  return cbor_item_unique_ptr(nullptr, delete_cbor_item);
}

cbor_item_unique_ptr wrap_cbor_item(cbor_item_t* item) {
  return cbor_item_unique_ptr(item, delete_cbor_item);
}

cbor_item_unique_ptr make_cbor_map(size_t size) {
  return cbor_item_unique_ptr(cbor_new_definite_map(size), delete_cbor_item);
}

cbor_item_unique_ptr make_cbor_array(size_t length) {
  return cbor_item_unique_ptr(cbor_new_definite_array(length),
                              delete_cbor_item);
}

cbor_item_unique_ptr make_cbor_int(int32_t n) {
  return cbor_item_unique_ptr(CborUtils::EncodeInt(n), delete_cbor_item);
}

cbor_item_unique_ptr make_cbor_string(const char* val) {
  return cbor_item_unique_ptr(cbor_build_string(val), delete_cbor_item);
}

cbor_item_unique_ptr make_cbor_bytestring(string_view string_view) {
  return cbor_item_unique_ptr(
      cbor_build_bytestring((unsigned char*)string_view.data(),
                            string_view.size()),
      delete_cbor_item);
}

cbor_item_t* move_out(cbor_item_unique_ptr& ptr) {
  cbor_item_t* item = ptr.get();
  // When the unique pointer releases the item, below, it will call
  // cbor_decref() on it, decreasing the ref count. Increase the ref count
  // so that cbor_decref() does not free the memory.
  cbor_incref(item);
  // This will undo the ref count bump, above.
  ptr.reset(nullptr);
  // This will undo the original ref count the item had when constructed or
  // wrapped. The container the pointer is passed to must take ownership of
  // the item and increase it's refcount.
  return cbor_move(item);
}

}  // namespace patch_subset::cbor
