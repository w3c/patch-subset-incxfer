#ifndef PATCH_SUBSET_CBOR_CBOR_ITEM_UNIQUE_PTR_H_
#define PATCH_SUBSET_CBOR_CBOR_ITEM_UNIQUE_PTR_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "cbor.h"

namespace patch_subset::cbor {

void delete_cbor_item(cbor_item_t* item);

typedef std::unique_ptr<cbor_item_t, decltype(&delete_cbor_item)>
    cbor_item_unique_ptr;

cbor_item_unique_ptr empty_cbor_ptr();

cbor_item_unique_ptr wrap_cbor_item(cbor_item_t* item);

cbor_item_unique_ptr make_cbor_map(int size);

cbor_item_unique_ptr make_cbor_array(int length);

cbor_item_unique_ptr make_cbor_int(int64_t n);

cbor_item_unique_ptr make_cbor_string(const char* val);

cbor_item_unique_ptr make_cbor_bytestring(absl::string_view string_view);

// Returns the contained item, with decremented ref count.
// The item should be passed to a container which will own the item.
// (The item may have a ref count of 0, which is not valid for cbor_decref()
// to free). The cbor_item_unique_ptr is left containing nullptr.
cbor_item_t* move_out(cbor_item_unique_ptr& ptr);

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CBOR_ITEM_UNIQUE_PTR_H_
