#ifndef PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
#define PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/patch_subset_server.h"

namespace patch_subset {

class MockPatchSubsetServer : public PatchSubsetServer {
 public:
  MOCK_METHOD(absl::StatusCode, Handle,
              (const std::string& font_id,
               const patch_subset::cbor::PatchRequest& request,
               patch_subset::cbor::PatchResponse& response /* OUT */),
              (override));
};

class ReturnResponse {
 public:
  explicit ReturnResponse(const patch_subset::cbor::PatchResponse& response)
      : response_(response) {}

  absl::StatusCode operator()(
      const std::string& font_id,
      const patch_subset::cbor::PatchRequest& request,
      patch_subset::cbor::PatchResponse& response /* OUT */) {
    response_.CopyTo(response);
    return absl::StatusCode::kOk;
  }

 private:
  const patch_subset::cbor::PatchResponse response_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
