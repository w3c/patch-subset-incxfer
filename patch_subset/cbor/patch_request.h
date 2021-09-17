#ifndef PATCH_SUBSET_CBOR_PATCH_REQUEST_H_
#define PATCH_SUBSET_CBOR_PATCH_REQUEST_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "common/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"
#include "patch_subset/cbor/compressed_set.h"
#include "patch_subset/constants.h"

namespace patch_subset::cbor {

/*
 * TODO: more doc
 * https://w3c.github.io/PFE/Overview.html#PatchRequest
 */
class PatchRequest {
 private:
  std::optional<ProtocolVersion> _protocol_version;
  std::optional<std::vector<patch_subset::PatchFormat>> _accept_formats;
  std::optional<CompressedSet> _codepoints_have;
  std::optional<CompressedSet> _codepoints_needed;
  std::optional<CompressedSet> _indices_have;
  std::optional<CompressedSet> _indices_needed;
  std::optional<uint64_t> _ordering_checksum;
  std::optional<uint64_t> _original_font_checksum;
  std::optional<uint64_t> _base_checksum;
  std::optional<ConnectionSpeed> _connection_speed;

 public:
  static const int kProtocolVersionFieldNumber = 0;
  static const int kAcceptPatchFormatsFieldNumber = 1;
  static const int kCodepointsHaveFieldNumber = 2;
  static const int kCodepointsNeededFieldNumber = 3;
  static const int kIndicesHaveFieldNumber = 4;
  static const int kIndicesNeededFieldNumber = 5;
  static const int kAxisSpaceHave = 6;
  static const int kAxisSpaceNeeded = 7;
  static const int kOrderingChecksumFieldNumber = 8;
  static const int kOriginalFontChecksumFieldNumber = 9;
  static const int kBaseChecksumFieldNumber = 10;
  static const int kConnectionSpeedFieldNumber = 11;

 public:
  PatchRequest();
  PatchRequest(const PatchRequest& other) = default;
  PatchRequest(PatchRequest&& other) noexcept;
  PatchRequest(ProtocolVersion protocol_version,
               std::vector<patch_subset::PatchFormat> accept_formats,
               CompressedSet codepoints_have, CompressedSet codepoints_needed,
               CompressedSet indices_have, CompressedSet indices_needed,
               uint64_t ordering_checksum, uint64_t original_font_checksum,
               uint64_t base_checksum, ConnectionSpeed connection_speed);

  static StatusCode Decode(const cbor_item_t& cbor_map, PatchRequest& out);
  StatusCode Encode(cbor_item_unique_ptr& map_out) const;

  static StatusCode ParseFromString(const std::string& buffer,
                                    PatchRequest& out);
  StatusCode SerializeToString(std::string& out) const;

  bool HasProtocolVersion() const;
  ProtocolVersion GetProtocolVersion() const;
  PatchRequest& SetProtocolVersion(ProtocolVersion version);
  PatchRequest& ResetProtocolVersion();

  bool HasAcceptFormats() const;
  const std::vector<patch_subset::PatchFormat>& AcceptFormats() const;
  PatchRequest& SetAcceptFormats(
      const std::vector<patch_subset::PatchFormat>& formats);
  PatchRequest& AddAcceptFormat(patch_subset::PatchFormat);
  PatchRequest& ResetAcceptFormats();

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

  bool HasConnectionSpeed() const;
  ConnectionSpeed GetConnectionSpeed() const;
  PatchRequest& SetConnectionSpeed(ConnectionSpeed connection_speed);
  PatchRequest& ResetConnectionSpeed();

  // Returns a human readable version of this PatchRequest.
  std::string ToString() const;

  PatchRequest& operator=(PatchRequest&& other) noexcept;
  bool operator==(const PatchRequest& other) const;
  bool operator!=(const PatchRequest& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_PATCH_REQUEST_H_
