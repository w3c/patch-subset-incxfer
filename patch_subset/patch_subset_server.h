#ifndef PATCH_SUBSET_PATCH_SUBSET_SERVER_H_
#define PATCH_SUBSET_PATCH_SUBSET_SERVER_H_

#include "common/status.h"
#include "patch_subset/patch_subset.pb.h"

namespace patch_subset {

// Interface for a PatchSubsetServer. This server processes
// PatchRequestProto's which request the generation of a patch
// which can extend a font subset.
class PatchSubsetServer {
 public:
  virtual ~PatchSubsetServer() = default;

  // Handle a patch request from a client. Writes the resulting response
  // into response.
  virtual StatusCode Handle(const std::string& font_id,
                            const PatchRequestProto& request,
                            PatchResponseProto* response /* OUT */) = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_SERVER_H_
