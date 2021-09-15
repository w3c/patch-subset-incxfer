#ifndef PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
#define PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_

#include "common/status.h"
#include "gtest/gtest.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/patch_subset_server.h"

using patch_subset::cbor::PatchRequest;

namespace patch_subset {

class MockPatchSubsetServer : public PatchSubsetServer {
 public:
  MOCK_METHOD(StatusCode, Handle,
              (const std::string& font_id, const PatchRequest& request,
               PatchResponseProto* response /* OUT */),
              (override));
};

class ReturnResponse {
 public:
  explicit ReturnResponse(const PatchResponseProto& response)
      : response_(response) {}

  StatusCode operator()(const std::string& font_id, const PatchRequest& request,
                        PatchResponseProto* response /* OUT */) {
    *response = response_;
    return StatusCode::kOk;
  }

 private:
  const PatchResponseProto& response_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
