#ifndef PATCH_SUBSET_CBOR_CBOR_UTILS_H_
#define PATCH_SUBSET_CBOR_CBOR_UTILS_H_

#include <optional>
#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

class CborUtils {
 public:
  // Sets out to point to the given field from a map, or sets it to nullptr if
  // not found.
  static StatusCode GetField(const cbor_item_t& cbor_map, int field_number,
                             cbor_item_unique_ptr& out);

  static StatusCode GetUInt64Field(const cbor_item_t& map, int field_number,
                                   std::optional<uint64_t>& out);
  static StatusCode GetStringField(const cbor_item_t& map, int field_number,
                                   std::optional<std::string>& out);
  static StatusCode GetBytesField(const cbor_item_t& map, int field_number,
                                  std::optional<std::string>& out);
  static StatusCode GetProtocolVersionField(
      const cbor_item_t& map, int field_number,
      std::optional<ProtocolVersion>& out);
  static StatusCode GetConnectionSpeedField(
      const cbor_item_t& map, int field_number,
      std::optional<ConnectionSpeed>& out);

  // Sets a field in a map.
  // Note: field_value must be a pointer, to work with CBOR library.
  static StatusCode SetField(cbor_item_t& cbor_map, int field_number,
                             cbor_item_t* field_value);

  static StatusCode SetUInt64Field(cbor_item_t& map, int field_number,
                                   const std::optional<uint64_t>& value);
  static StatusCode SetStringField(cbor_item_t& map, int field_number,
                                   const std::optional<std::string>& value);
  static StatusCode SetBytesField(cbor_item_t& map, int field_number,
                                  const std::optional<std::string>& value);
  static StatusCode SetProtocolVersionField(
      cbor_item_t& map, int field_number,
      const std::optional<ProtocolVersion>& value);
  static StatusCode SetConnectionSpeedField(
      cbor_item_t& map, int field_number,
      const std::optional<ConnectionSpeed>& value);

  static cbor_item_t* EncodeInt(int32_t n);
  static StatusCode DecodeInt(const cbor_item_t& int_element, int32_t* out);

  static cbor_item_t* EncodeUInt64(uint64_t n);
  static StatusCode DecodeUInt64(const cbor_item_t& int_element, uint64_t* out);

  static cbor_item_t* EncodeString(const std::string& s);
  static StatusCode DecodeString(const cbor_item_t& string_item,
                                 std::string& out);

  static cbor_item_t* EncodeBytes(const absl::string_view& bytes);
  static StatusCode DecodeBytes(const cbor_item_t& bytes_item,
                                std::string& out);

  static std::set<int> MapKeys(const cbor_item_t& map);

  static StatusCode SerializeToBytes(const cbor_item_t& item,
                                     absl::string_view buffer,
                                     size_t* bytes_written);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CBOR_UTILS_H_
