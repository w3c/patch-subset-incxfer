#ifndef PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
#define PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/binary_patch.h"
#include "common/font_data.h"
#include "common/hasher.h"
#include "hb.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/integer_list_checksum.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Client for interacting with a PatchSubsetServer. Produces the requests
// to be sent to a server and decodes responses from a server.
class PatchSubsetClient {
 public:
  // TODO(garretrieger): take a map of encoding to BinaryPatch instead of just
  // one encoding.
  explicit PatchSubsetClient(
      std::unique_ptr<common::BinaryPatch> binary_patch,
      std::unique_ptr<common::Hasher> hasher,
      std::unique_ptr<IntegerListChecksum> ordering_hasher)
      : binary_patch_(std::move(binary_patch)),
        hasher_(std::move(hasher)),
        ordering_hasher_(std::move(ordering_hasher)) {}

  absl::StatusOr<patch_subset::cbor::PatchRequest> CreateRequest(
      const hb_set_t& additional_codepoints,
      const common::FontData& font_subset) const;

  patch_subset::cbor::PatchRequest CreateRequest(
      const hb_set_t& codepoints_have, const hb_set_t& codepoints_needed,
      uint64_t base_checksum,
      const patch_subset::cbor::ClientState& state) const;

  absl::StatusOr<common::FontData> DecodeResponse(
      const common::FontData& font_subset,
      const common::FontData& encoded_response,
      const std::string& encoding) const;

 private:
  absl::Status EncodeCodepoints(const patch_subset::cbor::ClientState& state,
                                hb_set_t* codepoints_have,
                                hb_set_t* codepoints_needed) const;

  std::unique_ptr<common::BinaryPatch> binary_patch_;
  std::unique_ptr<common::Hasher> hasher_;
  std::unique_ptr<IntegerListChecksum> ordering_hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
