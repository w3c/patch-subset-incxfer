#ifndef PATCH_SUBSET_PATCH_SUBSET_SERVER_H_
#define PATCH_SUBSET_PATCH_SUBSET_SERVER_H_

#include "absl/status/status.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

// Interface for a PatchSubsetServer. This server processes
// PatchRequests which request the generation of a patch
// which can extend a font subset.
class PatchSubsetServer {
 public:
  virtual ~PatchSubsetServer() = default;

  // TODO(garretrieger): add structs to aggregate request and response details?

  // Handle a patch request from a client. Writes the resulting response
  // into response.
  virtual absl::Status Handle(const std::string& font_id,
                              const std::vector<std::string>& accept_encoding,
                              const patch_subset::cbor::PatchRequest& request,
                              patch_subset::FontData& response, /* OUT */
                              std::string& content_encoding /* OUT */) = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_SERVER_H_
