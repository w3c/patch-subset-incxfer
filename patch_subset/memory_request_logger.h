#ifndef PATCH_SUBSET_MEMORY_REQUEST_LOGGER_H_
#define PATCH_SUBSET_MEMORY_REQUEST_LOGGER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Implementation of RequestLogger that saves request/response
// stats to an inmemory buffer.
class MemoryRequestLogger : public RequestLogger {
 public:
  struct Record {
    uint32_t request_size;
    uint32_t response_size;
  };

  absl::Status LogRequest(const std::string& request_data,
                          const std::string& response_data) override;

  const std::vector<Record>& Records() const;

 private:
  std::vector<Record> records_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_MEMORY_REQUEST_LOGGER_H_
