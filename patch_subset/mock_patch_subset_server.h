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
               FontData& response /* OUT */,
               std::string& encoding /* OUT */),
              (override));
};

class ReturnResponse {
 public:
  explicit ReturnResponse(const FontData& response)
      : response_() {
    response_.copy(response.str());
  }

  absl::Status operator()(
      const std::string& font_id,
      const std::vector<std::string>& accept_encoding,
      const patch_subset::cbor::PatchRequest& request,
      FontData& response, /* OUT */
      std::string encoding /* OUT */) {
    response_.copy(response.str());
    encoding_ = encoding;
    return absl::OkStatus();
  }

 private:
  FontData response_;
  std::string encoding_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MOCK_PATCH_SUBSET_SERVER_H_
