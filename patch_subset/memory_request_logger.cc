#include "patch_subset/memory_request_logger.h"

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace patch_subset {

using absl::Status;

Status MemoryRequestLogger::LogRequest(const std::string& request_data,
                                       const std::string& response_data) {
  MemoryRequestLogger::Record record;
  record.request_size = request_data.size();
  record.response_size = response_data.size();
  records_.push_back(record);
  return absl::OkStatus();
}

const std::vector<MemoryRequestLogger::Record>& MemoryRequestLogger::Records()
    const {
  return records_;
}

}  // namespace patch_subset
