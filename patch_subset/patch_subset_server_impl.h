#ifndef PATCH_SUBSET_PATCH_SUBSET_SERVER_IMPL_H_
#define PATCH_SUBSET_PATCH_SUBSET_SERVER_IMPL_H_

#include <string>

#include "common/logging.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/binary_diff.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/codepoint_predictor.h"
#include "patch_subset/compressed_list_checksum.h"
#include "patch_subset/compressed_list_checksum_impl.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/font_provider.h"
#include "patch_subset/frequency_codepoint_predictor.h"
#include "patch_subset/harfbuzz_subsetter.h"
#include "patch_subset/hasher.h"
#include "patch_subset/noop_codepoint_predictor.h"
#include "patch_subset/patch_subset.pb.h"
#include "patch_subset/patch_subset_server.h"
#include "patch_subset/simple_codepoint_mapper.h"
#include "patch_subset/subsetter.h"

namespace patch_subset {

struct RequestState;

class ServerConfig {
 public:
  ServerConfig() {}

  // Location of the font library.
  std::string font_directory = "";

  // Location of unicode range data files
  std::string unicode_data_directory = "";

  // Maximum number of predicted codepoints to add to each request.
  int max_predicted_codepoints = 0;

  // Only add codepoints above this threshold [0.0 - 1.0]
  float prediction_frequency_threshold = 0.0f;

  // remap codepoints
  bool remap_codepoints = false;

  CodepointMapper* CreateCodepointMapper() const {
    if (remap_codepoints) {
      return new SimpleCodepointMapper();
    }
    return nullptr;
  }

  CompressedListChecksum* CreateMappingChecksum(Hasher* hasher) const {
    if (remap_codepoints) {
      return new CompressedListChecksumImpl(hasher);
    }
    return nullptr;
  }

  CodepointPredictor* CreateCodepointPredictor() const {
    if (!max_predicted_codepoints) {
      return reinterpret_cast<CodepointPredictor*>(
          new NoopCodepointPredictor());
    }

    CodepointPredictor* predictor = nullptr;
    if (unicode_data_directory.empty()) {
      predictor = reinterpret_cast<CodepointPredictor*>(
          FrequencyCodepointPredictor::Create(prediction_frequency_threshold));
    } else {
      predictor = reinterpret_cast<CodepointPredictor*>(
          FrequencyCodepointPredictor::Create(prediction_frequency_threshold,
                                              unicode_data_directory));
    }

    if (predictor) {
      return predictor;
    }

    LOG(WARNING) << "Failed to create codepoint predictor, using noop "
                    "predictor instead.";
    return new NoopCodepointPredictor();
  }
};

class PatchSubsetServerImpl : public PatchSubsetServer {
 public:
  static std::unique_ptr<PatchSubsetServer> CreateServer(
      const ServerConfig& config) {
    Hasher* hasher = new FastHasher();
    return std::unique_ptr<PatchSubsetServer>(new PatchSubsetServerImpl(
        config.max_predicted_codepoints,
        std::unique_ptr<FontProvider>(
            new FileFontProvider(config.font_directory)),
        std::unique_ptr<Subsetter>(new HarfbuzzSubsetter()),
        std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff()),
        std::unique_ptr<Hasher>(hasher),
        std::unique_ptr<CodepointMapper>(config.CreateCodepointMapper()),
        std::unique_ptr<CompressedListChecksum>(
            config.CreateMappingChecksum(hasher)),
        std::unique_ptr<CodepointPredictor>(
            config.CreateCodepointPredictor())));
  }

  // Takes ownership of font_provider, subsetter, and binary_diff.
  PatchSubsetServerImpl(
      int max_predicted_codepoints, std::unique_ptr<FontProvider> font_provider,
      std::unique_ptr<Subsetter> subsetter,
      std::unique_ptr<BinaryDiff> binary_diff, std::unique_ptr<Hasher> hasher,
      std::unique_ptr<CodepointMapper> codepoint_mapper,
      std::unique_ptr<CompressedListChecksum> compressed_list_checksum,
      std::unique_ptr<CodepointPredictor> codepoint_predictor)
      : max_predicted_codepoints_(max_predicted_codepoints),
        font_provider_(std::move(font_provider)),
        subsetter_(std::move(subsetter)),
        binary_diff_(std::move(binary_diff)),
        hasher_(std::move(hasher)),
        codepoint_mapper_(std::move(codepoint_mapper)),
        compressed_list_checksum_(std::move(compressed_list_checksum)),
        codepoint_predictor_(std::move(codepoint_predictor)) {}

  // Handle a patch request from a client. Writes the resulting response
  // into response.
  StatusCode Handle(const std::string& font_id,
                    const PatchRequestProto& request,
                    PatchResponseProto* response /* OUT */) override;

 private:
  void LoadInputCodepoints(const PatchRequestProto& request,
                           RequestState* state) const;

  void CheckOriginalFingerprint(uint64_t original_fingerprint,
                                RequestState* state) const;

  StatusCode ComputeCodepointRemapping(RequestState* state) const;

  void AddCodepointRemapping(const RequestState& state,
                             CompressedListProto* response) const;

  void AddPredictedCodepoints(RequestState* state) const;

  StatusCode ComputeSubsets(const std::string& font_id,
                            RequestState* state) const;

  void ValidatePatchBase(uint64_t base_fingerprint, RequestState* state) const;

  void ConstructResponse(const RequestState& state,
                         PatchResponseProto* response) const;

  StatusCode ValidateFingerPrint(uint64_t fingerprint,
                                 const FontData& data) const;

  void AddFingerprints(const FontData& font_data, const FontData& target_subset,
                       PatchResponseProto* response) const;

  void AddFingerprints(const FontData& font_data,
                       PatchResponseProto* response) const;

  bool Check(StatusCode result) const;
  bool Check(StatusCode result, const std::string& message) const;

  int max_predicted_codepoints_;
  std::unique_ptr<FontProvider> font_provider_;
  std::unique_ptr<Subsetter> subsetter_;
  std::unique_ptr<BinaryDiff> binary_diff_;
  std::unique_ptr<Hasher> hasher_;
  std::unique_ptr<CodepointMapper> codepoint_mapper_;
  std::unique_ptr<CompressedListChecksum> compressed_list_checksum_;
  std::unique_ptr<CodepointPredictor> codepoint_predictor_;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_PATCH_SUBSET_SERVER_IMPL_H_
