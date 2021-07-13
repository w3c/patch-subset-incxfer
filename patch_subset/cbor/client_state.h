#ifndef PATCH_SUBSET_CBOR_CLIENT_STATE_H_
#define PATCH_SUBSET_CBOR_CLIENT_STATE_H_

#include <optional>
#include <vector>

#include "cbor.h"
#include "common/status.h"
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
  std::optional<uint64_t> _fingerprint;
  std::optional<std::vector<int32_t>> _codepoint_remapping;

  static const int kFontIdFieldNumber = 0;
  static const int kFontDataFieldNumber = 1;
  static const int kFingerprintFieldNumber = 2;
  static const int kCodepointRemappingFieldNumber = 3;

 public:
  ClientState();
  ClientState(const std::string& font_id, const std::string& font_data,
              uint64_t fingerprint,
              const std::vector<int32_t>& codepoint_remapping);

  static StatusCode Decode(const cbor_item_t& cbor_map, ClientState& out);

  ClientState& SetFontId(const std::string& font_id);
  ClientState& ResetFontId();
  [[nodiscard]] bool HasFontId() const;
  [[nodiscard]] std::string FontId() const;

  ClientState& SetFontData(const std::string& font_data);
  ClientState& ResetFontData();
  [[nodiscard]] bool HasFontData() const;
  [[nodiscard]] std::string FontData() const;

  ClientState& SetFingerprint(uint64_t);
  ClientState& ResetFingerprint();
  [[nodiscard]] bool HasFingerprint() const;
  [[nodiscard]] uint64_t Fingerprint() const;

  ClientState& SetCodepointRemapping(
      const std::vector<int32_t>& codepoint_remapping);
  ClientState& ResetCodepointRemapping();
  [[nodiscard]] bool HasCodepointRemapping() const;
  [[nodiscard]] std::vector<int32_t> CodepointRemapping() const;

  StatusCode Encode(cbor_item_unique_ptr& out) const;

  bool operator==(const ClientState& other) const;
  bool operator!=(const ClientState& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CLIENT_STATE_H_
