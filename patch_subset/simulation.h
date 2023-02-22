#ifndef PATCH_SUBSET_SIMULATION_H_
#define PATCH_SUBSET_SIMULATION_H_

#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hasher.h"
#include "patch_subset/patch_subset_server.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Helper for simulating the interaction of a client and server.
class Simulation {
 public:
  // PatchSubsetClient does not take ownership of request_logger or server.
  // request_logger and server must remain alive as long as PatchSubsetClient
  // is alive.
  explicit Simulation(PatchSubsetClient* client, PatchSubsetServer* server,
                      RequestLogger* request_logger)
      : client_(client), server_(server), request_logger_(request_logger) {}

  absl::StatusOr<FontData> Extend(const hb_set_t& additional_codepoints,
                                  const FontData& font_subset);

 private:
  void LogRequest(const patch_subset::cbor::PatchRequest& request,
                  const patch_subset::cbor::PatchResponse& response);

  PatchSubsetClient* client_;
  PatchSubsetServer* server_;
  RequestLogger* request_logger_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_SIMULATION_H_
