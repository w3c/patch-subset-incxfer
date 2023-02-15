#ifndef PATCH_SUBSET_NULL_REQUEST_LOGGER_H_
#define PATCH_SUBSET_NULL_REQUEST_LOGGER_H_

#include "absl/status/status.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Implementation of RequestLogger that does nothing.
class NullRequestLogger : public RequestLogger {
 public:
  absl::Status LogRequest(const std::string& request_data,
                          const std::string& response_data) override {
    // Do nothing.
    return absl::OkStatus();
  }
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_NULL_REQUEST_LOGGER_H_
