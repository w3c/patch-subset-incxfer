#ifndef PATCH_SUBSET_NULL_REQUEST_LOGGER_H_
#define PATCH_SUBSET_NULL_REQUEST_LOGGER_H_

#include "common/status.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Implementation of RequestLogger that does nothing.
class NullRequestLogger : public RequestLogger {
 public:
  StatusCode LogRequest(const std::string& request_data,
                        const std::string& response_data) override {
    // Do nothing.
    return StatusCode::kOk;
  }
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_NULL_REQUEST_LOGGER_H_
