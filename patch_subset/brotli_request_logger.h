#ifndef PATCH_SUBSET_BROTLI_REQUEST_LOGGER_H_
#define PATCH_SUBSET_BROTLI_REQUEST_LOGGER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/memory_request_logger.h"
#include "patch_subset/request_logger.h"

namespace patch_subset {

// Implementation of RequestLogger that saves applies a pass
// of brotli compression to the request/response data if
// it results in a smaller size, then logs the compressed
// size to an inmemory buffer.
class BrotliRequestLogger : public RequestLogger {
 public:
  BrotliRequestLogger(MemoryRequestLogger* memory_request_logger)
      : memory_request_logger_(memory_request_logger),
        brotli_diff_(new BrotliBinaryDiff()) {}

  absl::StatusCode LogRequest(const std::string& request_data,
                        const std::string& response_data) override;

 private:
  absl::StatusCode CompressIfSmaller(const std::string& data,
                               std::string* output_data);

  MemoryRequestLogger* memory_request_logger_;
  std::unique_ptr<BrotliBinaryDiff> brotli_diff_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_BROTLI_REQUEST_LOGGER_H_
