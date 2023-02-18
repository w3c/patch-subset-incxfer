#ifndef PATCH_SUBSET_CBOR_CLIENT_STATE_H_
#define PATCH_SUBSET_CBOR_CLIENT_STATE_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "cbor.h"
#include "patch_subset/cbor/axis_space.h"
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
  std::optional<uint64_t> _original_font_checksum;
  std::optional<std::vector<int32_t>> _codepoint_ordering;
  std::optional<AxisSpace> _subset_axis_space;
  std::optional<AxisSpace> _original_axis_space;

 public:
  // See: https://w3c.github.io/IFT/Overview.html#ClientState
  static const int kOriginalFontChecksumFieldNumber = 0;
  static const int kCodepointOrderingFieldNumber = 1;
  static const int kSubsetAxisSpaceFieldNumber = 2;
  static const int kOriginalAxisSpaceFieldNumber = 3;
  // TODO(garretrieger): add (kOriginalFeaturesFieldNumber = 4).

  ClientState();
  ClientState(const ClientState& other) = default;
  ClientState(ClientState&& other) noexcept;

  ClientState(uint64_t original_font_checksum,
              const std::vector<int32_t>& codepoint_ordering,
              const AxisSpace& subset_axis_space,
              const AxisSpace& original_axis_space);

  static absl::Status Decode(const cbor_item_t& cbor_map, ClientState& out);
  absl::Status Encode(cbor_item_unique_ptr& out) const;

  static absl::Status ParseFromString(const std::string& buffer,
                                      ClientState& out);
  absl::Status SerializeToString(std::string& out) const;


  ClientState& SetOriginalFontChecksum(uint64_t);
  ClientState& ResetOriginalFontChecksum();
  [[nodiscard]] bool HasOriginalFontChecksum() const;
  [[nodiscard]] uint64_t OriginalFontChecksum() const;

  ClientState& SetCodepointOrdering(
      const std::vector<int32_t>& codepoint_ordering);
  ClientState& ResetCodepointOrdering();
  [[nodiscard]] bool HasCodepointOrdering() const;
  [[nodiscard]] const std::vector<int32_t>& CodepointOrdering() const;

  ClientState& SetSubsetAxisSpace(const AxisSpace& space);
  ClientState& ResetSubsetAxisSpace();
  [[nodiscard]] bool HasSubsetAxisSpace() const;
  [[nodiscard]] const AxisSpace& SubsetAxisSpace() const;

  ClientState& SetOriginalAxisSpace(const AxisSpace& patch);
  ClientState& ResetOriginalAxisSpace();
  [[nodiscard]] bool HasOriginalAxisSpace() const;
  [[nodiscard]] const AxisSpace& OriginalAxisSpace() const;

  // Returns a human readable version of this ClientState.
  std::string ToString() const;

  ClientState& operator=(ClientState&& other) noexcept;
  bool operator==(const ClientState& other) const;
  bool operator!=(const ClientState& other) const;
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_CLIENT_STATE_H_
