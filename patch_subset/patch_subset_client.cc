#include "patch_subset/patch_subset_client.h"

#include "common/logging.h"
#include "common/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/patch_subset.pb.h"

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
    PatchRequestProto* request) {
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
                                     ClientState* state) {
  PatchRequestProto request;
  StatusCode result = CreateRequest(additional_codepoints, *state, &request);
  if (result != StatusCode::kOk ||
      CompressedSet::IsEmpty(request.codepoints_needed())) {
    return result;
  }

  PatchResponseProto response;
  result = server_->Handle(state->FontId(), request, &response);
  if (result != StatusCode::kOk) {
    LOG(WARNING) << "Got a failure from the patch subset server (code = "
                 << result << ").";
    return result;
  }

  result = AmendState(response, state);
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

StatusCode PatchSubsetClient::ComputePatched(const PatchResponseProto& response,
                                             const ClientState& state,
                                             FontData* patched) {
  if (response.patch().empty() && response.replacement().empty()) {
    // TODO(garretrieger): implement support.
    LOG(WARNING) << "Re-indexing is not yet implemented.";
    return StatusCode::kUnimplemented;
  }

  FontData base;
  if (!response.patch().empty()) {
    base.copy(state.FontData());
  }

  if (response.format() != PatchFormat::BROTLI_SHARED_DICT) {
    LOG(WARNING) << "Unsupported patch format " << response.format();
    return StatusCode::kFailedPrecondition;
  }

  FontData patch_data;
  if (!response.patch().empty() && !response.replacement().empty()) {
    return StatusCode::kUnimplemented;
  } else if (!response.patch().empty()) {
    patch_data.copy(response.patch());
  } else {
    patch_data.copy(response.replacement());
  }

  binary_patch_->Patch(base, patch_data, patched);

  if (hasher_->Checksum(patched->str()) != response.patched_fingerprint()) {
    LOG(WARNING) << "Patched checksum mismatch.";
    return StatusCode::kFailedPrecondition;
  }

  return StatusCode::kOk;
}

StatusCode PatchSubsetClient::AmendState(const PatchResponseProto& response,
                                         ClientState* state) {
  FontData patched;
  StatusCode result = ComputePatched(response, *state, &patched);
  if (result != StatusCode::kOk) {
    return result;
  }

  state->SetFontData(patched.string());
  state->SetOriginalFontChecksum(response.original_font_fingerprint());

  if (response.has_codepoint_ordering()) {
    // Temporary, will be converted.
    CodepointMap map;
    map.FromProto(response.codepoint_ordering());
    vector<int32_t> remapping;
    StatusCode sc = map.ToVector(&remapping);
    if (sc != StatusCode::kOk) {
      return sc;
    }
    state->SetCodepointRemapping(remapping);
    state->SetCodepointRemappingChecksum(response.ordering_checksum());
  }

  return StatusCode::kOk;
}

void PatchSubsetClient::CreateRequest(const hb_set_t& codepoints_have,
                                      const hb_set_t& codepoints_needed,
                                      const ClientState& state,
                                      PatchRequestProto* request) {
  if (hb_set_get_population(&codepoints_have)) {
    request->set_original_font_fingerprint(state.OriginalFontChecksum());
    request->set_base_fingerprint(hasher_->Checksum(state.FontData()));
  }
  request->add_accept_format(PatchFormat::BROTLI_SHARED_DICT);

  CompressedSetProto codepoints_have_encoded;
  CompressedSet::Encode(codepoints_have, &codepoints_have_encoded);
  *request->mutable_codepoints_have() = codepoints_have_encoded;

  CompressedSetProto codepoints_needed_encoded;
  CompressedSet::Encode(codepoints_needed, &codepoints_needed_encoded);
  *request->mutable_codepoints_needed() = codepoints_needed_encoded;

  if (!state.CodepointRemapping().empty()) {
    request->set_index_fingerprint(state.CodepointRemappingChecksum());
  }
}

void PatchSubsetClient::LogRequest(const PatchRequestProto& request,
                                   const PatchResponseProto& response) {
  std::string request_bytes;
  request.SerializeToString(&request_bytes);
  std::string response_bytes;
  response.SerializeToString(&response_bytes);
  request_logger_->LogRequest(request_bytes, response_bytes);
}

}  // namespace patch_subset
