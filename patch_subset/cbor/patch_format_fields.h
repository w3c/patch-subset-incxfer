#ifndef PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_
#define PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "absl/status/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

/*
 * // TODO doc
 * See https://w3c.github.io/PFE/Overview.html#patch-formats
 */
class PatchFormatFields {
 public:
  static absl::StatusCode ToPatchFormat(uint64_t value,
                                  patch_subset::PatchFormat* out);

  static absl::StatusCode Decode(const cbor_item_t& bytes,
                           std::vector<patch_subset::PatchFormat>& out);

  static absl::StatusCode Encode(
      const std::vector<patch_subset::PatchFormat>& formats,
      cbor_item_unique_ptr& bytestring_out);

  static absl::StatusCode SetPatchFormatsListField(
      cbor_item_t& map, int field_number,
      const std::optional<std::vector<patch_subset::PatchFormat>>& format_list);
  static absl::StatusCode GetPatchFormatsListField(
      const cbor_item_t& map, int field_number,
      std::optional<std::vector<patch_subset::PatchFormat>>& out);

  static absl::StatusCode SetPatchFormatField(
      cbor_item_t& map, int field_number,
      const std::optional<patch_subset::PatchFormat>& format_list);
  static absl::StatusCode GetPatchFormatField(
      const cbor_item_t& map, int field_number,
      std::optional<patch_subset::PatchFormat>& out);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_PATCH_FORMAT_FIELDS_H_
