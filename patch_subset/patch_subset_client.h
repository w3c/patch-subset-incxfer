#ifndef PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
#define PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_

#include "common/status.h"
#include "hb.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hasher.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/patch_subset_server.h"
#include "patch_subset/request_logger.h"

using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;

namespace patch_subset {

// Client for interacting with a PatchSubsetServer. Allows
// for extending a subset of a font by getting a patch
// from the server.
class PatchSubsetClient {
 public:
  // PatchSubsetClient does not take ownership of request_logger or server.
  // request_logger and server must remain alive as long as PatchSubsetClient
  // is alive.
  explicit PatchSubsetClient(PatchSubsetServer* server,
                             RequestLogger* request_logger,
                             std::unique_ptr<BinaryPatch> binary_patch,
                             std::unique_ptr<Hasher> hasher)
      : server_(server),
        request_logger_(request_logger),
        binary_patch_(std::move(binary_patch)),
        hasher_(std::move(hasher)) {}

  StatusCode Extend(const hb_set_t& additional_codepoints, ClientState* state);

  StatusCode CreateRequest(const hb_set_t& additional_codepoints,
                           const ClientState& state, PatchRequest* request);

  StatusCode AmendState(const PatchResponseProto& response, ClientState* state);

 private:
  StatusCode EncodeCodepoints(const ClientState& state,
                              hb_set_t* codepoints_have,
                              hb_set_t* codepoints_needed);

  void CreateRequest(const hb_set_t& codepoints_have,
                     const hb_set_t& codepoints_needed,
                     const ClientState& state, PatchRequest* request);

  void LogRequest(const PatchRequest& request,
                  const PatchResponseProto& response);

  StatusCode ComputePatched(const PatchResponseProto& response,
                            const ClientState& state, FontData* patched);

  PatchSubsetServer* server_;
  RequestLogger* request_logger_;
  std::unique_ptr<BinaryPatch> binary_patch_;
  std::unique_ptr<Hasher> hasher_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_
