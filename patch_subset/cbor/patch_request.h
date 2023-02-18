#ifndef PATCH_SUBSET_CBOR_PATCH_REQUEST_H_
#define PATCH_SUBSET_CBOR_PATCH_REQUEST_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/compressed_set.h"

namespace patch_subset::cbor {

/*
 * TODO: more doc
 * https://w3c.github.io/PFE/Overview.html#PatchRequest
 */
class PatchRequest {
 private:
  std::optional<CompressedSet> _codepoints_have;
  std::optional<CompressedSet> _codepoints_needed;
  std::optional<CompressedSet> _indices_have;
  std::optional<CompressedSet> _indices_needed;
  std::optional<uint64_t> _ordering_checksum;
  std::optional<uint64_t> _original_font_checksum;
  std::optional<uint64_t> _base_checksum;

 public:
  static const int kCodepointsHaveFieldNumber = 0;
  static const int kCodepointsNeededFieldNumber = 1;
  static const int kIndicesHaveFieldNumber = 2;
  static const int kIndicesNeededFieldNumber = 3;
  static const int kFeaturesHaveFieldNumber = 4;
  static const int kFeaturesNeededFieldNumber = 5;
  static const int kAxisSpaceHave = 6;
  static const int kAxisSpaceNeeded = 7;
  static const int kOrderingChecksumFieldNumber = 8;
  static const int kOriginalFontChecksumFieldNumber = 9;
  static const int kBaseChecksumFieldNumber = 10;

 public:
  PatchRequest();
  PatchRequest(const PatchRequest& other) = default;
  PatchRequest(PatchRequest&& other) noexcept;
  PatchRequest(CompressedSet codepoints_have, CompressedSet codepoints_needed,
               CompressedSet indices_have, CompressedSet indices_needed,
               uint64_t ordering_checksum, uint64_t original_font_checksum,
               uint64_t base_checksum);

  static absl::Status Decode(const cbor_item_t& cbor_map, PatchRequest& out);
  absl::Status Encode(cbor_item_unique_ptr& map_out) const;

  static absl::Status ParseFromString(const std::string& buffer,
                                      PatchRequest& out);
  absl::Status SerializeToString(std::string& out) const;

  bool HasCodepointsHave() const;
  const CompressedSet& CodepointsHave() const;
  PatchRequest& SetCodepointsHave(const CompressedSet& codepoints);
  PatchRequest& ResetCodepointsHave();

  bool HasCodepointsNeeded() const;
  const CompressedSet& CodepointsNeeded() const;
  PatchRequest& SetCodepointsNeeded(const CompressedSet& codepoints);
  PatchRequest& ResetCodepointsNeeded();

  bool HasIndicesHave() const;
  const CompressedSet& IndicesHave() const;
  PatchRequest& SetIndicesHave(const CompressedSet& indices);
  PatchRequest& ResetIndicesHave();

  bool HasIndicesNeeded() const;
  const CompressedSet& IndicesNeeded() const;
  PatchRequest& SetIndicesNeeded(const CompressedSet& indices);
  PatchRequest& ResetIndicesNeeded();

  bool HasOrderingChecksum() const;
  uint64_t OrderingChecksum() const;
  PatchRequest& SetOrderingChecksum(uint64_t checksum);
  PatchRequest& ResetOrderingChecksum();

  bool HasOriginalFontChecksum() const;
  uint64_t OriginalFontChecksum() const;
  PatchRequest& SetOriginalFontChecksum(uint64_t checksum);
  PatchRequest& ResetOriginalFontChecksum();

  bool HasBaseChecksum() const;
  uint64_t BaseChecksum() const;
  PatchRequest& SetBaseChecksum(uint64_t checksum);
  PatchRequest& ResetBaseChecksum();

  // Returns a human readable version of this PatchRequest.
  std::string ToString() const;

  PatchRequest& operator=(PatchRequest&& other) noexcept;
  bool operator==(const PatchRequest& other) const;
  bool operator!=(const PatchRequest& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_PATCH_REQUEST_H_
