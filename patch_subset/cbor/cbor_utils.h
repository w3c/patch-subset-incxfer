#ifndef PATCH_SUBSET_CBOR_CBOR_UTILS_H_
#define PATCH_SUBSET_CBOR_CBOR_UTILS_H_

#include <optional>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

class CborUtils {
 public:
  // Sets out to point to the given field from a map, or sets it to nullptr if
  // not found.
  static absl::Status GetField(const cbor_item_t& cbor_map, int field_number,
                               cbor_item_unique_ptr& out);

  static absl::Status GetUInt64Field(const cbor_item_t& map, int field_number,
                                     std::optional<uint64_t>& out);
  static absl::Status GetFloatField(const cbor_item_t& map, int field_number,
                                    std::optional<float>& out);
  static absl::Status GetStringField(const cbor_item_t& map, int field_number,
                                     std::optional<std::string>& out);
  static absl::Status GetBytesField(const cbor_item_t& map, int field_number,
                                    std::optional<std::string>& out);

  // Sets a field in a map.
  // Note: field_value must be a pointer, to work with CBOR library.
  static absl::Status SetField(cbor_item_t& cbor_map, int field_number,
                               cbor_item_t* field_value);

  static absl::Status SetUInt64Field(cbor_item_t& map, int field_number,
                                     const std::optional<uint64_t>& value);
  static absl::Status SetFloatField(cbor_item_t& map, int field_number,
                                    const std::optional<float>& value);
  static absl::Status SetStringField(cbor_item_t& map, int field_number,
                                     const std::optional<std::string>& value);
  static absl::Status SetBytesField(cbor_item_t& map, int field_number,
                                    const std::optional<std::string>& value);

  static cbor_item_t* EncodeInt(int32_t n);
  static absl::Status DecodeInt(const cbor_item_t& int_element, int32_t* out);

  static cbor_item_t* EncodeUInt64(uint64_t n);
  static absl::Status DecodeUInt64(const cbor_item_t& int_element,
                                   uint64_t* out);

  static cbor_item_t* EncodeFloat(float n);
  static absl::Status DecodeFloat(const cbor_item_t& float_element, float* out);

  static cbor_item_t* EncodeString(const std::string& s);
  static absl::Status DecodeString(const cbor_item_t& string_item,
                                   std::string& out);

  static cbor_item_t* EncodeBytes(const absl::string_view& bytes);
  static absl::Status DecodeBytes(const cbor_item_t& bytes_item,
                                  std::string& out);

  static std::set<uint64_t> MapKeys(const cbor_item_t& map);

  static absl::Status SerializeToBytes(const cbor_item_t& item,
                                       absl::string_view buffer,
                                       size_t* bytes_written);
  static absl::Status DeserializeFromBytes(absl::string_view buffer,
                                           cbor_item_unique_ptr& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CBOR_UTILS_H_
