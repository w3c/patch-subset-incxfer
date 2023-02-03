#include "patch_subset/patch_subset_client.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::Status;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;

void CodepointsInFont(const std::string& font_data, hb_set_t* codepoints) {
  hb_blob_t* blob = hb_blob_create(font_data.c_str(), font_data.size(),
                                   HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  hb_face_collect_unicodes(face, codepoints);
  hb_face_destroy(face);
}

Status PatchSubsetClient::CreateRequest(
    const hb_set_t& additional_codepoints, const ClientState& state,
    PatchRequest* request) {
  hb_set_unique_ptr existing_codepoints = make_hb_set();
  CodepointsInFont(state.FontData(), existing_codepoints.get());

  hb_set_unique_ptr new_codepoints = make_hb_set();
  hb_set_union(new_codepoints.get(), &additional_codepoints);
  hb_set_subtract(new_codepoints.get(), existing_codepoints.get());

  Status result =
      EncodeCodepoints(state, existing_codepoints.get(), new_codepoints.get());
  if (!result.ok()) {
    return result;
  }

  if (!hb_set_get_population(new_codepoints.get())) {
    // No new codepoints are needed. No action needed.
    return absl::OkStatus();
  }

  CreateRequest(*existing_codepoints, *new_codepoints, state, request);
  return absl::OkStatus();
}

Status PatchSubsetClient::Extend(const hb_set_t& additional_codepoints,
                                 ClientState& state) {
  PatchRequest request;
  Status result = CreateRequest(additional_codepoints, state, &request);
  if (!result.ok() ||
      (request.IndicesNeeded().empty() && request.CodepointsNeeded().empty())) {
    return result;
  }

  PatchResponse response;
  result = server_->Handle(state.FontId(), request, response);
  if (!result.ok()) {
    return result;
  }

  result = AmendState(response, &state);
  if (!result.ok()) {
    return result;
  }

  LogRequest(request, response);
  return absl::OkStatus();
}

Status PatchSubsetClient::EncodeCodepoints(const ClientState& state,
                                           hb_set_t* codepoints_have,
                                           hb_set_t* codepoints_needed) {
  if (state.CodepointRemapping().empty()) {
    return absl::OkStatus();
  }

  CodepointMap map;
  map.FromVector(state.CodepointRemapping());

  Status result;
  map.IntersectWithMappedCodepoints(codepoints_have);
  if (!(result = map.Encode(codepoints_have)).ok()) {
    LOG(WARNING) << "Failed to encode codepoints_have with the mapping.";
    return result;
  }

  map.IntersectWithMappedCodepoints(codepoints_needed);
  if (!(result = map.Encode(codepoints_needed)).ok()) {
    LOG(WARNING) << "Failed to encode codepoints_needed with the mapping.";
    return result;
  }

  return absl::OkStatus();
}

Status PatchSubsetClient::ComputePatched(const PatchResponse& response,
                                         const ClientState* state,
                                         FontData* patched) {
  if (response.Patch().empty() && response.Replacement().empty()) {
    // TODO(garretrieger): implement support.
    return absl::UnimplementedError("Re-indexing is not yet implemented.");
  }

  FontData base;
  if (!response.Patch().empty()) {
    base.copy(state->FontData());
  }

  if (response.GetPatchFormat() != PatchFormat::BROTLI_SHARED_DICT) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unsupported patch format ", response.GetPatchFormat()));
  }

  FontData patch_data;
  if (!response.Patch().empty() && !response.Replacement().empty()) {
    return absl::UnimplementedError("Resend handling not implemented yet.");
  } else if (!response.Patch().empty()) {
    patch_data.copy(response.Patch());
  } else {
    patch_data.copy(response.Replacement());
  }

  Status s = binary_patch_->Patch(base, patch_data, patched);
  if (!s.ok()) {
    return s;
  }

  if (hasher_->Checksum(patched->str()) != response.PatchedChecksum()) {
    return absl::FailedPreconditionError("Patched checksum mismatch.");
  }

  return absl::OkStatus();
}

Status PatchSubsetClient::AmendState(const PatchResponse& response,
                                     ClientState* state) {
  FontData patched;
  Status result = ComputePatched(response, state, &patched);
  if (!result.ok()) {
    return result;
  }

  state->SetFontData(patched.string());
  state->SetOriginalFontChecksum(response.OriginalFontChecksum());

  if (response.HasCodepointOrdering()) {
    state->SetCodepointRemapping(response.CodepointOrdering());
    state->SetCodepointRemappingChecksum(response.OrderingChecksum());
  }

  return absl::OkStatus();
}

void PatchSubsetClient::CreateRequest(const hb_set_t& codepoints_have,
                                      const hb_set_t& codepoints_needed,
                                      const ClientState& state,
                                      PatchRequest* request) {
  request->SetProtocolVersion(ProtocolVersion::ONE);
  if (hb_set_get_population(&codepoints_have)) {
    request->SetOriginalFontChecksum(state.OriginalFontChecksum());
    request->SetBaseChecksum(hasher_->Checksum(state.FontData()));
  }
  request->AddAcceptFormat(PatchFormat::BROTLI_SHARED_DICT);

  if (!hb_set_is_empty(&codepoints_have)) {
    patch_subset::cbor::CompressedSet codepoints_have_encoded;
    CompressedSet::Encode(codepoints_have, codepoints_have_encoded);

    if (state.CodepointRemapping().empty()) {
      request->SetCodepointsHave(codepoints_have_encoded);
    } else {
      request->SetIndicesHave(codepoints_have_encoded);
    }
  }

  if (!hb_set_is_empty(&codepoints_needed)) {
    patch_subset::cbor::CompressedSet codepoints_needed_encoded;
    CompressedSet::Encode(codepoints_needed, codepoints_needed_encoded);

    if (state.CodepointRemapping().empty()) {
      request->SetCodepointsNeeded(codepoints_needed_encoded);
    } else {
      request->SetIndicesNeeded(codepoints_needed_encoded);
    }
  }

  if (!state.CodepointRemapping().empty()) {
    request->SetOrderingChecksum(state.CodepointRemappingChecksum());
  }
}

void PatchSubsetClient::LogRequest(const PatchRequest& request,
                                   const PatchResponse& response) {
  std::string request_bytes;
  request.SerializeToString(request_bytes);
  std::string response_bytes;
  response.SerializeToString(response_bytes);
  Status result = request_logger_->LogRequest(request_bytes, response_bytes);
  if (!result.ok()) {
    LOG(WARNING) << "Error logging result: " << result;
  }
}

}  // namespace patch_subset
