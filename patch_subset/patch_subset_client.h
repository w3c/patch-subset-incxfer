#ifndef PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
#define PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hasher.h"
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
      std::unique_ptr<BinaryPatch> binary_patch, std::unique_ptr<Hasher> hasher,
      std::unique_ptr<IntegerListChecksum> ordering_hasher)
      : binary_patch_(std::move(binary_patch)),
        hasher_(std::move(hasher)),
        ordering_hasher_(std::move(ordering_hasher)) {}

  absl::StatusOr<patch_subset::cbor::PatchRequest> CreateRequest(
      const hb_set_t& additional_codepoints, const FontData& font_subset) const;

  absl::StatusOr<FontData> DecodeResponse(const FontData& font_subset,
                                          const FontData& encoded_response,
                                          const std::string& encoding) const;

 private:
  absl::Status EncodeCodepoints(const patch_subset::cbor::ClientState& state,
                                hb_set_t* codepoints_have,
                                hb_set_t* codepoints_needed) const;

  patch_subset::cbor::PatchRequest CreateRequest(
      const hb_set_t& codepoints_have, const hb_set_t& codepoints_needed,
      const FontData& font_subset,
      const patch_subset::cbor::ClientState& state) const;

  std::unique_ptr<BinaryPatch> binary_patch_;
  std::unique_ptr<Hasher> hasher_;
  std::unique_ptr<IntegerListChecksum> ordering_hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
