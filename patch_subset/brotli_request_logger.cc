#include "patch_subset/brotli_request_logger.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

using absl::Status;

Status BrotliRequestLogger::LogRequest(const std::string& request_data,
                                       const std::string& response_data) {
  std::string compressed_request_data;
  std::string compressed_response_data;
  Status result = CompressIfSmaller(request_data, &compressed_request_data);
  result.Update(CompressIfSmaller(response_data, &compressed_response_data));
  if (!result.ok()) {
    return result;
  }

  return memory_request_logger_->LogRequest(compressed_request_data,
                                            compressed_response_data);
}

Status BrotliRequestLogger::CompressIfSmaller(const std::string& data,
                                              std::string* output_data) {
  FontData empty;
  FontData compressed;
  FontData font_data(data);

  Status s = brotli_diff_->Diff(empty, font_data, &compressed);
  if (!s.ok()) {
    return s;
  }

  if (compressed.size() < data.size()) {
    output_data->assign(compressed.data(), compressed.size());
  } else {
    *output_data = data;
  }

  return absl::OkStatus();
}

}  // namespace patch_subset
