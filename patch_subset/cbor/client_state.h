#ifndef PATCH_SUBSET_CBOR_CLIENT_STATE_H_
#define PATCH_SUBSET_CBOR_CLIENT_STATE_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "absl/status/status.h"
#include "patch_subset/cbor/cbor_item_unique_ptr.h"

namespace patch_subset::cbor {

/*
 * The data that a client needs to maintain in order issue a series of
 * requests and handle responses. This is not included in requests or
 * responses. Clients are free to their data in another way. This class provides
 * convenient serialization to and from bytes.
 */
class ClientState {
 private:
  std::optional<std::string> _font_id;
  std::optional<std::string> _font_data;
  std::optional<uint64_t> _original_font_checksum;
  std::optional<std::vector<int32_t>> _codepoint_remapping;
  std::optional<uint64_t> _codepoint_remapping_checksum;

  static const int kFontIdFieldNumber = 0;
  static const int kFontDataFieldNumber = 1;
  static const int kOriginalFontChecksumFieldNumber = 2;
  static const int kCodepointRemappingFieldNumber = 3;
  static const int kCodepointRemappingChecksumFieldNumber = 4;

 public:
  ClientState();
  ClientState(const ClientState& other) = default;
  ClientState(ClientState&& other) noexcept;
  ClientState(const std::string& font_id, const std::string& font_data,
              uint64_t original_font_checksum,
              const std::vector<int32_t>& codepoint_remapping,
              uint64_t codepoint_remapping_checksum);

  static absl::StatusCode Decode(const cbor_item_t& cbor_map, ClientState& out);
  absl::StatusCode Encode(cbor_item_unique_ptr& out) const;

  static absl::StatusCode ParseFromString(const std::string& buffer,
                                    ClientState& out);
  absl::StatusCode SerializeToString(std::string& out) const;

  ClientState& SetFontId(const std::string& font_id);
  ClientState& ResetFontId();
  [[nodiscard]] bool HasFontId() const;
  [[nodiscard]] const std::string& FontId() const;

  ClientState& SetFontData(const std::string& font_data);
  ClientState& ResetFontData();
  [[nodiscard]] bool HasFontData() const;
  [[nodiscard]] const std::string& FontData() const;

  ClientState& SetOriginalFontChecksum(uint64_t);
  ClientState& ResetOriginalFontChecksum();
  [[nodiscard]] bool HasOriginalFontChecksum() const;
  [[nodiscard]] uint64_t OriginalFontChecksum() const;

  ClientState& SetCodepointRemapping(
      const std::vector<int32_t>& codepoint_remapping);
  ClientState& ResetCodepointRemapping();
  [[nodiscard]] bool HasCodepointRemapping() const;
  [[nodiscard]] const std::vector<int32_t>& CodepointRemapping() const;

  ClientState& SetCodepointRemappingChecksum(uint64_t);
  ClientState& ResetCodepointRemappingChecksum();
  [[nodiscard]] bool HasCodepointRemappingChecksum() const;
  [[nodiscard]] uint64_t CodepointRemappingChecksum() const;

  // Returns a human readable version of this ClientState.
  std::string ToString() const;

  ClientState& operator=(ClientState&& other) noexcept;
  bool operator==(const ClientState& other) const;
  bool operator!=(const ClientState& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CLIENT_STATE_H_
