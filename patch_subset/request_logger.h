#ifndef PATCH_SUBSET_REQUEST_LOGGER_H_
#define PATCH_SUBSET_REQUEST_LOGGER_H_

#include <string>

#include "common/status.h"

namespace patch_subset {

// Client for interacting with a PatchSubsetServer. Allows
// for extending a subset of a font by getting a patch
// from the server.
class RequestLogger {
 public:
  virtual ~RequestLogger() = default;

  virtual StatusCode LogRequest(const std::string& request_data,
                                const std::string& response_data) = 0;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_REQUEST_LOGGER_H_
