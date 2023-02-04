#ifndef PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_
#define PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

/*
 * // TODO doc
 * See https://w3c.github.io/PFE/Overview.html#patch-formats
 */
class PatchFormatFields {
 public:
  static absl::Status ToPatchFormat(uint64_t value,
                                        patch_subset::PatchFormat* out);

  static absl::Status Decode(const cbor_item_t& bytes,
                                 std::vector<patch_subset::PatchFormat>& out);

  static absl::Status Encode(
      const std::vector<patch_subset::PatchFormat>& formats,
      cbor_item_unique_ptr& bytestring_out);

  static absl::Status SetPatchFormatsListField(
      cbor_item_t& map, int field_number,
      const std::optional<std::vector<patch_subset::PatchFormat>>& format_list);
  static absl::Status GetPatchFormatsListField(
      const cbor_item_t& map, int field_number,
      std::optional<std::vector<patch_subset::PatchFormat>>& out);

  static absl::Status SetPatchFormatField(
      cbor_item_t& map, int field_number,
      const std::optional<patch_subset::PatchFormat>& format_list);
  static absl::Status GetPatchFormatField(
      const cbor_item_t& map, int field_number,
      std::optional<patch_subset::PatchFormat>& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_
