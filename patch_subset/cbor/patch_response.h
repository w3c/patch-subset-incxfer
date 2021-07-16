#ifndef PATCH_SUBSET_CBOR_PATCH_RESPONSE_H_
#define PATCH_SUBSET_CBOR_PATCH_RESPONSE_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

/*
 * TODO: More doc
 * https://w3c.github.io/PFE/Overview.html#PatchResponse
 */
class PatchResponse {
 private:
  std::optional<ProtocolVersion> _protocol_version;
  std::optional<PatchFormat> _patch_format;
  std::optional<std::string> _patch;
  std::optional<std::string> _replacement;
  std::optional<uint64_t> _original_font_checksum;
  std::optional<uint64_t> _patched_checksum;
  std::optional<std::vector<int32_t>> _codepoint_ordering;
  std::optional<uint64_t> _ordering_checksum;

  static const int kProtocolVersionFieldNumber = 0;
  static const int kPatchFormatFieldNumber = 1;
  static const int kPatchFieldNumber = 2;
  static const int kReplacementFieldNumber = 3;
  static const int kOriginalFontChecksumFieldNumber = 4;
  static const int kPatchedChecksumFieldNumber = 5;
  static const int kCodepointOrderingFieldNumber = 6;
  static const int kOrderingChecksumFieldNumber = 7;

 public:
  PatchResponse();
  PatchResponse(const PatchResponse& other) = default;
  PatchResponse(PatchResponse&& other);
  PatchResponse(ProtocolVersion protocol_version, PatchFormat patch_format,
                std::string patch, std::string replacement,
                uint64_t original_font_checksum, uint64_t patched_checksum,
                std::vector<int32_t> codepoint_ordering,
                uint64_t ordering_checksum);

  static StatusCode Decode(const cbor_item_t& cbor_map, PatchResponse& out);

  StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  bool HasProtocolVersion() const;
  ProtocolVersion GetProtocolVersion() const;
  PatchResponse& SetProtocolVersion(ProtocolVersion protocol_version);
  PatchResponse& ResetProtocolVersion();

  bool HasPatchFormat() const;
  PatchFormat GetPatchFormat() const;
  PatchResponse& SetPatchFormat(PatchFormat format);
  PatchResponse& ResetPatchFormat();

  bool HasPatch() const;
  const std::string& Patch() const;
  PatchResponse& SetPatch(std::string patch);
  PatchResponse& ResetPatch();

  bool HasReplacement() const;
  const std::string& Replacement() const;
  PatchResponse& SetReplacement(std::string replacement);
  PatchResponse& ResetReplacement();

  bool HasOriginalFontChecksum() const;
  uint64_t OriginalFontChecksum() const;
  PatchResponse& SetOriginalFontChecksum(uint64_t checksum);
  PatchResponse& ResetOriginalFontChecksum();

  bool HasPatchedChecksum() const;
  uint64_t PatchedChecksum() const;
  PatchResponse& SetPatchedChecksum(uint64_t checksum);
  PatchResponse& ResetPatchedChecksum();

  bool HasCodepointOrdering() const;
  const std::vector<int32_t>& CodepointOrdering() const;
  PatchResponse& SetCodepointOrdering(std::vector<int32_t> codepoint_ordering);
  PatchResponse& ResetCodepointOrdering();

  bool HasOrderingChecksum() const;
  uint64_t OrderingChecksum() const;
  PatchResponse& SetOrderingChecksum(uint64_t checksum);
  PatchResponse& ResetOrderingChecksum();

  PatchResponse& operator=(PatchResponse&& other);
  bool operator==(const PatchResponse& other) const;
  bool operator!=(const PatchResponse& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_PATCH_RESPONSE_H_
