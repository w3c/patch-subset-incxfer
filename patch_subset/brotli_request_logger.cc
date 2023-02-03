#include "patch_subset/brotli_request_logger.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"

namespace patch_subset {

using absl::StatusCode;

StatusCode BrotliRequestLogger::LogRequest(const std::string& request_data,
                                           const std::string& response_data) {
  std::string compressed_request_data;
  StatusCode result = CompressIfSmaller(request_data, &compressed_request_data);
  if (result != StatusCode::kOk) {
    return result;
  }

  std::string compressed_response_data;
  CompressIfSmaller(response_data, &compressed_response_data);
  if (result != StatusCode::kOk) {
    return result;
  }

  return memory_request_logger_->LogRequest(compressed_request_data,
                                            compressed_response_data);
}

StatusCode BrotliRequestLogger::CompressIfSmaller(const std::string& data,
                                                  std::string* output_data) {
  FontData empty;
  FontData compressed;
  FontData font_data(data);

  StatusCode result = brotli_diff_->Diff(empty, font_data, &compressed);
  if (result != StatusCode::kOk) {
    return result;
  }

  if (compressed.size() < data.size()) {
    output_data->assign(compressed.data(), compressed.size());
  } else {
    *output_data = data;
  }

  return StatusCode::kOk;
}

}  // namespace patch_subset
