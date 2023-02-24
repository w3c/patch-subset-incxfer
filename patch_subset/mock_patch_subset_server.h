#ifndef PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
#define PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/patch_subset_server.h"

namespace patch_subset {

class MockPatchSubsetServer : public PatchSubsetServer {
 public:
  MOCK_METHOD(absl::Status, Handle,
              (const std::string& font_id,
               const std::vector<std::string>& accept_encoding,
               const patch_subset::cbor::PatchRequest& request,
               FontData& response /* OUT */),
              // TODO encoding
              (override));
};

class ReturnResponse {
 public:
  explicit ReturnResponse(const patch_subset::cbor::PatchResponse& response)
      : response_(response) {}

  absl::Status operator()(
      const std::string& font_id,
      const std::vector<std::string>& accept_encoding,
      const patch_subset::cbor::PatchRequest& request,
      FontData& response /* OUT */) {
    response_.copy(response);
    return absl::OkStatus();
  }

 private:
  const FontData response_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
