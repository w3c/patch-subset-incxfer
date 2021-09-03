#include "patch_subset/patch_subset_server_impl.h"

#include <stdio.h>

#include "absl/strings/string_view.h"
#include "common/logging.h"
#include "hb-subset.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/patch_subset.pb.h"

using ::absl::string_view;

namespace patch_subset {

// Helper object, which holds all of the relevant state for
// handling a single request.
struct RequestState {
  RequestState()
      : codepoints_have(make_hb_set()),
        codepoints_needed(make_hb_set()),
        mapping(),
        codepoint_mapping_invalid(false) {}

  bool IsPatch() const {
    return !IsReindex() && !hb_set_is_empty(codepoints_have.get());
  }

  bool IsRebase() const { return !IsReindex() && !IsPatch(); }

  bool IsReindex() const { return codepoint_mapping_invalid; }

  hb_set_unique_ptr codepoints_have;
  hb_set_unique_ptr codepoints_needed;
  uint64_t index_fingerprint;
  CodepointMap mapping;
  FontData font_data;
  FontData client_subset;
  FontData client_target_subset;
  FontData patch;
  bool codepoint_mapping_invalid;
};

StatusCode PatchSubsetServerImpl::Handle(const std::string& font_id,
                                         const PatchRequestProto& request,
                                         PatchResponseProto* response) {
  RequestState state;

  LoadInputCodepoints(request, &state);

  StatusCode result;
  if (!Check(result = font_provider_->GetFont(font_id, &state.font_data),
             "Failed to load font (font_id = " + font_id + ").")) {
    return result;
  }

  CheckOriginalFingerprint(request.original_font_fingerprint(), &state);

  if (codepoint_mapper_) {
    if (!Check(result = ComputeCodepointRemapping(&state))) {
      return result;
    }
  }

  AddPredictedCodepoints(&state);

  if (state.IsReindex()) {
    ConstructResponse(state, response);
    return StatusCode::kOk;
  }

  if (!Check(result = ComputeSubsets(font_id, &state))) {
    return result;
  }

  ValidatePatchBase(request.base_fingerprint(), &state);

  if (!Check(result = binary_diff_->Diff(
                 state.client_subset, state.client_target_subset, &state.patch),
             "Diff computation failed (font_id = " + font_id + ").")) {
    return result;
  }

  // TODO(garretrieger): rename the proto package.
  // TODO(garretrieger): check which diffs the client supports.
  // TODO(garretrieger): handle exceptional cases (see design doc).

  ConstructResponse(state, response);

  return StatusCode::kOk;
}

void PatchSubsetServerImpl::LoadInputCodepoints(
    const PatchRequestProto& request, RequestState* state) const {
  CompressedSet::Decode(request.codepoints_have(),
                        state->codepoints_have.get());
  CompressedSet::Decode(request.codepoints_needed(),
                        state->codepoints_needed.get());
  hb_set_union(state->codepoints_needed.get(), state->codepoints_have.get());
  state->index_fingerprint = request.index_fingerprint();
}

void PatchSubsetServerImpl::CheckOriginalFingerprint(
    uint64_t original_fingerprint, RequestState* state) const {
  if (state->IsPatch() &&
      !Check(ValidateFingerPrint(original_fingerprint, state->font_data),
             "Client's original fingerprint does not match. Switching to "
             "REBASE.")) {
    hb_set_clear(state->codepoints_have.get());
  }
}

StatusCode PatchSubsetServerImpl::ComputeCodepointRemapping(
    RequestState* state) const {
  hb_set_unique_ptr codepoints = make_hb_set();
  subsetter_->CodepointsInFont(state->font_data, codepoints.get());
  codepoint_mapper_->ComputeMapping(codepoints.get(), &state->mapping);

  if (state->IsRebase()) {
    // Don't remap input codepoints for a rebase request (client isn't
    // using the mapping yet.)
    return StatusCode::kOk;
  }

  CompressedListProto compressed_list_proto;
  if (!Check(state->mapping.ToProto(&compressed_list_proto),
             "Invalid codepoint mapping. Unable to convert to proto.")) {
    // This typically shouldn't happen, so bail with internal error.
    return StatusCode::kInternal;
  }

  uint64_t expected_checksum =
      compressed_list_checksum_->Checksum(compressed_list_proto);
  if (expected_checksum != state->index_fingerprint) {
    LOG(WARNING) << "Client index finger print (" << state->index_fingerprint
                 << ") does not match expected finger print ("
                 << expected_checksum << "). Sending a REINDEX response.";
    state->codepoint_mapping_invalid = true;
    return StatusCode::kOk;
  }

  // Codepoints given to use by the client are using the computed codepoint
  // mapping, so translate the provided sets back to actual codepoints.
  state->mapping.Decode(state->codepoints_have.get());
  state->mapping.Decode(state->codepoints_needed.get());
  return StatusCode::kOk;
}

void PatchSubsetServerImpl::AddCodepointRemapping(
    const RequestState& state, CompressedListProto* ordering) const {
  state.mapping.ToProto(ordering);
}

void PatchSubsetServerImpl::AddPredictedCodepoints(RequestState* state) const {
  hb_set_unique_ptr codepoints_in_font = make_hb_set();
  hb_set_unique_ptr codepoints_being_added = make_hb_set();

  subsetter_->CodepointsInFont(state->font_data, codepoints_in_font.get());

  hb_set_union(codepoints_being_added.get(), state->codepoints_needed.get());
  hb_set_subtract(codepoints_being_added.get(), state->codepoints_have.get());
  hb_set_unique_ptr additional_codepoints = make_hb_set();

  codepoint_predictor_->Predict(
      codepoints_in_font.get(), state->codepoints_have.get(),
      codepoints_being_added.get(), max_predicted_codepoints_,
      additional_codepoints.get());

  hb_set_union(state->codepoints_needed.get(), additional_codepoints.get());
}

StatusCode PatchSubsetServerImpl::ComputeSubsets(const std::string& font_id,
                                                 RequestState* state) const {
  StatusCode result = subsetter_->Subset(
      state->font_data, *state->codepoints_have, &state->client_subset);
  if (result != StatusCode::kOk) {
    LOG(WARNING) << "Subsetting for client_subset "
                 << "(font_id = " << font_id << ")"
                 << "failed.";
    return result;
  }

  result = subsetter_->Subset(state->font_data, *state->codepoints_needed,
                              &state->client_target_subset);
  if (result != StatusCode::kOk) {
    LOG(WARNING) << "Subsetting for client_target_subset "
                 << "(font_id = " << font_id << ")"
                 << "failed.";
    return result;
  }

  return result;
}

void PatchSubsetServerImpl::ValidatePatchBase(uint64_t base_fingerprint,
                                              RequestState* state) const {
  if (state->IsPatch() &&
      !Check(ValidateFingerPrint(base_fingerprint, state->client_subset),
             "Client's base does not match. Switching to REBASE.")) {
    // Clear the client_subset since it doesn't match. The diff will then diff
    // in rebase mode.
    state->client_subset.reset();
    hb_set_clear(state->codepoints_have.get());
  }
}

void PatchSubsetServerImpl::ConstructResponse(
    const RequestState& state, PatchResponseProto* response) const {
  if ((state.IsReindex() || state.IsRebase()) && codepoint_mapper_) {
    AddCodepointRemapping(state, response->mutable_codepoint_ordering());
    response->set_ordering_checksum(
        compressed_list_checksum_->Checksum(response->codepoint_ordering()));
  }

  if (state.IsReindex()) {
    AddFingerprints(state.font_data, response);
    // Early return, no patch is needed for a re-index.
    return;
  }

  response->set_format(PatchFormat::BROTLI_SHARED_DICT);
  if (state.IsPatch()) {
    response->set_patch(state.patch.data(), state.patch.size());
  } else if (state.IsRebase()) {
    response->set_replacement(state.patch.data(), state.patch.size());
  }

  AddFingerprints(state.font_data, state.client_target_subset, response);
}

StatusCode PatchSubsetServerImpl::ValidateFingerPrint(
    uint64_t fingerprint, const FontData& data) const {
  uint64_t actual_fingerprint = hasher_->Checksum(data.str());
  if (actual_fingerprint != fingerprint) {
    LOG(WARNING) << "Finger print mismatch. "
                 << "Expected = " << fingerprint << " "
                 << "Actual = " << actual_fingerprint << ".";
    return StatusCode::kInvalidArgument;
  }
  return StatusCode::kOk;
}

void PatchSubsetServerImpl::AddFingerprints(
    const FontData& font_data, const FontData& target_subset,
    PatchResponseProto* response) const {
  response->set_original_font_fingerprint(
      hasher_->Checksum(string_view(font_data.str())));
  response->set_patched_fingerprint(
      hasher_->Checksum(string_view(target_subset.str())));
}

void PatchSubsetServerImpl::AddFingerprints(
    const FontData& font_data, PatchResponseProto* response) const {
  response->set_original_font_fingerprint(
      hasher_->Checksum(string_view(font_data.str())));
}

bool PatchSubsetServerImpl::Check(StatusCode result) const {
  return result == StatusCode::kOk;
}

bool PatchSubsetServerImpl::Check(StatusCode result,
                                  const std::string& message) const {
  if (result == StatusCode::kOk) {
    return true;
  }
  LOG(WARNING) << message;
  return false;
}

}  // namespace patch_subset
