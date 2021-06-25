/*
 * Implements a C API interface into a patch subset client.
 * Needed in order to have python create and interact with
 * patch subset clients.
 */
#include <string>

#include "common/status.h"
#include "hb.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/brotli_request_logger.h"
#include "patch_subset/farm_hasher.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/font_provider.h"
#include "patch_subset/memory_request_logger.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/patch_subset_client.h"
#include "patch_subset/patch_subset_server.h"
#include "patch_subset/patch_subset_server_impl.h"

using ::patch_subset::BinaryPatch;
using ::patch_subset::BrotliBinaryPatch;
using ::patch_subset::BrotliRequestLogger;
using ::patch_subset::ClientState;
using ::patch_subset::CodepointMapper;
using ::patch_subset::CodepointMappingChecksum;
using ::patch_subset::FarmHasher;
using ::patch_subset::FileFontProvider;
using ::patch_subset::FontProvider;
using ::patch_subset::Hasher;
using ::patch_subset::MemoryRequestLogger;
using ::patch_subset::PatchSubsetClient;
using ::patch_subset::PatchSubsetServer;
using ::patch_subset::PatchSubsetServerImpl;
using ::patch_subset::ServerConfig;
using ::patch_subset::StatusCode;
using ::patch_subset::Subsetter;

class PatchSubsetSession {
 public:
  PatchSubsetSession(const ServerConfig& config, const std::string& font_id)
      : binary_patch_(new BrotliBinaryPatch()),
        brotli_request_logger_(&request_logger_),
        server_(std::move(PatchSubsetServerImpl::CreateServer(config))),
        client_(server_.get(), &brotli_request_logger_,
                std::unique_ptr<BinaryPatch>(binary_patch_),
                std::unique_ptr<Hasher>(new FarmHasher())) {
    client_state_.set_font_id(font_id);
  }

  StatusCode Extend(const hb_set_t& codepoints) {
    return client_.Extend(codepoints, &client_state_);
  }

  const std::string& ClientFontData() const {
    return client_state_.font_data();
  }

  const std::vector<MemoryRequestLogger::Record>& GetRecords() const {
    return request_logger_.Records();
  }

  BinaryPatch* binary_patch_;
  MemoryRequestLogger request_logger_;
  BrotliRequestLogger brotli_request_logger_;
  std::unique_ptr<PatchSubsetServer> server_;
  PatchSubsetClient client_;
  ClientState client_state_;
};

extern "C" {

PatchSubsetSession* PatchSubsetSession_new(
    const char* font_directory, const char* font_id,
    bool with_codepoint_remapping, int32_t max_predicted_codepoints,
    float prediction_frequency_threshold) {
  std::string font_directory_string(font_directory);
  std::string font_id_string(font_id);

  ServerConfig config;
  config.font_directory = font_directory_string;
  config.remap_codepoints = with_codepoint_remapping;
  config.max_predicted_codepoints = max_predicted_codepoints;
  config.prediction_frequency_threshold = prediction_frequency_threshold;

  return new PatchSubsetSession(config, font_id);
}

void PatchSubsetSession_delete(PatchSubsetSession* session) { delete session; }

bool PatchSubsetSession_extend(PatchSubsetSession* session,
                               uint32_t* codepoints,
                               uint32_t codepoints_count) {
  hb_set_t* codepoints_set = hb_set_create();
  for (uint32_t i = 0; i < codepoints_count; i++) {
    hb_set_add(codepoints_set, codepoints[i]);
  }
  StatusCode result = session->Extend(*codepoints_set);
  hb_set_destroy(codepoints_set);
  return result == StatusCode::kOk;
}

const char* PatchSubsetSession_get_font(PatchSubsetSession* session,
                                        uint32_t* size) {
  *size = session->ClientFontData().size();
  return session->ClientFontData().c_str();
}

const MemoryRequestLogger::Record* PatchSubsetSession_get_requests(
    PatchSubsetSession* session, uint32_t* size) {
  *size = session->GetRecords().size();
  return session->GetRecords().data();
}
}
