#include "patch_subset/simulation.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::Status;
using patch_subset::cbor::PatchRequest;

StatusOr<FontData> Simulation::Extend(const hb_set_t& additional_codepoints,
                                      const FontData& font_subset) {
  auto request = client_->CreateRequest(additional_codepoints, font_subset);
  if (!request.ok()) {
    return request.status();
  }

  if (request->IndicesNeeded().empty() && request->CodepointsNeeded().empty()) {
    return font_subset;
  }

  FontData response;
  std::string encoding;
  auto status =
      server_->Handle(state.FontId(), {Encodings::kBrotliDiffEncoding},
                      *request, response, encoding);
  if (!status.ok()) {
    return status;
  }

  auto new_subset =
      client_->DecodeResponse(response, encoding) if (!new_subset.ok()) {
    return new_subset.status();
  }

  LogRequest(request, response);
  return *new_subset;
}

void Simulation::LogRequest(const PatchRequest& request,
                            const FontData& response) {
  std::string request_bytes;
  Status result = request.SerializeToString(request_bytes);
  std::string response_bytes = response.string();

  if (result.ok()) {
    result.Update(request_logger_->LogRequest(request_bytes, response_bytes));
  }

  if (!result.ok()) {
    LOG(WARNING) << "Error logging result: " << result;
  }
}

}  // namespace patch_subset
