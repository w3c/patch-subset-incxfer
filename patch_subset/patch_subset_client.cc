#include "patch_subset/patch_subset_client.h"

#include "common/logging.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/hb_set_unique_ptr.h"

using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;

namespace patch_subset {

void CodepointsInFont(const std::string& font_data, hb_set_t* codepoints) {
  hb_blob_t* blob = hb_blob_create(font_data.c_str(), font_data.size(),
                                   HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  hb_face_collect_unicodes(face, codepoints);
  hb_face_destroy(face);
}

StatusCode PatchSubsetClient::CreateRequest(
    const hb_set_t& additional_codepoints, const ClientState& state,
    PatchRequest* request) {
  hb_set_unique_ptr existing_codepoints = make_hb_set();
  CodepointsInFont(state.FontData(), existing_codepoints.get());

  hb_set_unique_ptr new_codepoints = make_hb_set();
  hb_set_union(new_codepoints.get(), &additional_codepoints);
  hb_set_subtract(new_codepoints.get(), existing_codepoints.get());

  StatusCode result =
      EncodeCodepoints(state, existing_codepoints.get(), new_codepoints.get());
  if (result != StatusCode::kOk) {
    return result;
  }

  if (!hb_set_get_population(new_codepoints.get())) {
    // No new codepoints are needed. No action needed.
    return StatusCode::kOk;
  }

  CreateRequest(*existing_codepoints, *new_codepoints, state, request);
  return StatusCode::kOk;
}

StatusCode PatchSubsetClient::Extend(const hb_set_t& additional_codepoints,
                                     ClientState& state) {
  PatchRequest request;
  StatusCode result = CreateRequest(additional_codepoints, state, &request);
  if (result != StatusCode::kOk ||
      (request.IndicesNeeded().empty() && request.CodepointsNeeded().empty())) {
    return result;
  }

  PatchResponse response;
  result = server_->Handle(state.FontId(), request, response);
  if (result != StatusCode::kOk) {
    LOG(WARNING) << "Got a failure from the patch subset server (code = "
                 << result << ").";
    return result;
  }

  result = AmendState(response, &state);
  if (result != StatusCode::kOk) {
    return result;
  }

  LogRequest(request, response);
  return StatusCode::kOk;
}

StatusCode PatchSubsetClient::EncodeCodepoints(const ClientState& state,
                                               hb_set_t* codepoints_have,
                                               hb_set_t* codepoints_needed) {
  if (state.CodepointRemapping().empty()) {
    return StatusCode::kOk;
  }

  CodepointMap map;
  map.FromVector(state.CodepointRemapping());

  StatusCode result;
  map.IntersectWithMappedCodepoints(codepoints_have);
  if ((result = map.Encode(codepoints_have)) != StatusCode::kOk) {
    LOG(WARNING) << "Failed to encode codepoints_have with the mapping.";
    return result;
  }

  map.IntersectWithMappedCodepoints(codepoints_needed);
  if ((result = map.Encode(codepoints_needed)) != StatusCode::kOk) {
    LOG(WARNING) << "Failed to encode codepoints_needed with the mapping.";
    return result;
  }

  return StatusCode::kOk;
}

StatusCode PatchSubsetClient::ComputePatched(const PatchResponse& response,
                                             const ClientState* state,
                                             FontData* patched) {
  if (response.Patch().empty() && response.Replacement().empty()) {
    // TODO(garretrieger): implement support.
    LOG(WARNING) << "Re-indexing is not yet implemented.";
    return StatusCode::kUnimplemented;
  }

  FontData base;
  if (!response.Patch().empty()) {
    base.copy(state->FontData());
  }

  if (response.GetPatchFormat() != PatchFormat::BROTLI_SHARED_DICT) {
    LOG(WARNING) << "Unsupported patch format " << response.GetPatchFormat();
    return StatusCode::kFailedPrecondition;
  }

  FontData patch_data;
  if (!response.Patch().empty() && !response.Replacement().empty()) {
    return StatusCode::kUnimplemented;
  } else if (!response.Patch().empty()) {
    patch_data.copy(response.Patch());
  } else {
    patch_data.copy(response.Replacement());
  }

  binary_patch_->Patch(base, patch_data, patched);

  if (hasher_->Checksum(patched->str()) != response.PatchedChecksum()) {
    LOG(WARNING) << "Patched checksum mismatch.";
    return StatusCode::kFailedPrecondition;
  }

  return StatusCode::kOk;
}

StatusCode PatchSubsetClient::AmendState(const PatchResponse& response,
                                         ClientState* state) {
  FontData patched;
  StatusCode result = ComputePatched(response, state, &patched);
  if (result != StatusCode::kOk) {
    return result;
  }

  state->SetFontData(patched.string());
  state->SetOriginalFontChecksum(response.OriginalFontChecksum());

  if (response.HasCodepointOrdering()) {
    state->SetCodepointRemapping(response.CodepointOrdering());
    state->SetCodepointRemappingChecksum(response.OrderingChecksum());
  }

  return StatusCode::kOk;
}

void PatchSubsetClient::CreateRequest(const hb_set_t& codepoints_have,
                                      const hb_set_t& codepoints_needed,
                                      const ClientState& state,
                                      PatchRequest* request) {
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
  request_logger_->LogRequest(request_bytes, response_bytes);
}

}  // namespace patch_subset
