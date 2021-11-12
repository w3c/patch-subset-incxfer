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

using ::absl::string_view;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;
using std::vector;

namespace patch_subset {

// Helper object, which holds all of the relevant state for
// handling a single request.
struct RequestState {
  RequestState()
      : codepoints_have(make_hb_set()),
        codepoints_needed(make_hb_set()),
        mapping(),
        codepoint_mapping_invalid(false),
        format(PatchFormat::VCDIFF) {}

  bool IsPatch() const {
    return !IsReindex() && !hb_set_is_empty(codepoints_have.get());
  }

  bool IsRebase() const { return !IsReindex() && !IsPatch(); }

  bool IsReindex() const { return codepoint_mapping_invalid; }

  hb_set_unique_ptr codepoints_have;
  hb_set_unique_ptr codepoints_needed;
  uint64_t ordering_checksum;
  CodepointMap mapping;
  FontData font_data;
  FontData client_subset;
  FontData client_target_subset;
  FontData patch;
  bool codepoint_mapping_invalid;
  PatchFormat format;
};

StatusCode PatchSubsetServerImpl::Handle(const std::string& font_id,
                                         const PatchRequest& request,
                                         PatchResponse& response) {
  RequestState state;

  LoadInputCodepoints(request, &state);

  StatusCode result;
  if (!Check(result = font_provider_->GetFont(font_id, &state.font_data),
             "Failed to load font (font_id = " + font_id + ").")) {
    return result;
  }

  CheckOriginalChecksum(request.OriginalFontChecksum(), &state);

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

  ValidatePatchBase(request.BaseChecksum(), &state);

  const BinaryDiff* binary_diff =
      DiffFor(request.AcceptFormats(), state.format);
  if (!binary_diff) {
    LOG(WARNING) << "No available binary diff algorithms were specified.";
    return StatusCode::kInvalidArgument;
  }
  if (!Check(result = binary_diff->Diff(
                 state.client_subset, state.client_target_subset, &state.patch),
             "Diff computation failed (font_id = " + font_id + ").")) {
    return result;
  }

  // TODO(garretrieger): handle exceptional cases (see design doc).

  ConstructResponse(state, response);

  return StatusCode::kOk;
}

void PatchSubsetServerImpl::LoadInputCodepoints(const PatchRequest& request,
                                                RequestState* state) const {
  CompressedSet::Decode(request.CodepointsHave(), state->codepoints_have.get());
  CompressedSet::Decode(request.CodepointsNeeded(),
                        state->codepoints_needed.get());
  hb_set_union(state->codepoints_needed.get(), state->codepoints_have.get());
  state->ordering_checksum = request.OrderingChecksum();
}

void PatchSubsetServerImpl::CheckOriginalChecksum(uint64_t original_checksum,
                                                  RequestState* state) const {
  if (state->IsPatch() &&
      !Check(ValidateChecksum(original_checksum, state->font_data),
             "Client's original checksum does not match. Switching to "
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

  vector<int32_t> mapping_ints;
  if (!Check(state->mapping.ToVector(&mapping_ints),
             "Invalid codepoint mapping. Unable to convert to vector.")) {
    // This typically shouldn't happen, so bail with internal error.
    return StatusCode::kInternal;
  }

  uint64_t expected_checksum = integer_list_checksum_->Checksum(mapping_ints);
  if (expected_checksum != state->ordering_checksum) {
    LOG(WARNING) << "Client ordering checksum (" << state->ordering_checksum
                 << ") does not match expected checksum (" << expected_checksum
                 << "). Sending a REINDEX response.";
    state->codepoint_mapping_invalid = true;
    return StatusCode::kOk;
  }

  // Codepoints given to use by the client are using the computed codepoint
  // mapping, so translate the provided sets back to actual codepoints.
  state->mapping.Decode(state->codepoints_have.get());
  state->mapping.Decode(state->codepoints_needed.get());
  return StatusCode::kOk;
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

void PatchSubsetServerImpl::ValidatePatchBase(uint64_t base_checksum,
                                              RequestState* state) const {
  if (state->IsPatch() &&
      !Check(ValidateChecksum(base_checksum, state->client_subset),
             "Client's base does not match. Switching to REBASE.")) {
    // Clear the client_subset since it doesn't match. The diff will then diff
    // in rebase mode.
    state->client_subset.reset();
    hb_set_clear(state->codepoints_have.get());
  }
}

void PatchSubsetServerImpl::ConstructResponse(const RequestState& state,
                                              PatchResponse& response) const {
  response.SetProtocolVersion(ProtocolVersion::ONE);
  if ((state.IsReindex() || state.IsRebase()) && codepoint_mapper_) {
    vector<int32_t> ordering;
    state.mapping.ToVector(&ordering);
    response.SetCodepointOrdering(ordering);
    response.SetOrderingChecksum(integer_list_checksum_->Checksum(ordering));
  }

  if (state.IsReindex()) {
    AddChecksums(state.font_data, response);
    // Early return, no patch is needed for a re-index.
    return;
  }

  response.SetPatchFormat(state.format);
  if (state.IsPatch()) {
    response.SetPatch(state.patch.string());
  } else if (state.IsRebase()) {
    response.SetReplacement(state.patch.string());
  }

  AddChecksums(state.font_data, state.client_target_subset, response);
}

StatusCode PatchSubsetServerImpl::ValidateChecksum(uint64_t checksum,
                                                   const FontData& data) const {
  uint64_t actual_checksum = hasher_->Checksum(data.str());
  if (actual_checksum != checksum) {
    LOG(WARNING) << "Checksum mismatch. "
                 << "Expected = " << checksum << " "
                 << "Actual = " << actual_checksum << ".";
    return StatusCode::kInvalidArgument;
  }
  return StatusCode::kOk;
}

void PatchSubsetServerImpl::AddChecksums(const FontData& font_data,
                                         const FontData& target_subset,
                                         PatchResponse& response) const {
  response.SetOriginalFontChecksum(
      hasher_->Checksum(string_view(font_data.str())));
  response.SetPatchedChecksum(
      hasher_->Checksum(string_view(target_subset.str())));
}

void PatchSubsetServerImpl::AddChecksums(const FontData& font_data,
                                         PatchResponse& response) const {
  response.SetOriginalFontChecksum(
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

const BinaryDiff* PatchSubsetServerImpl::DiffFor(
    const std::vector<PatchFormat>& formats,
    PatchFormat& format /* OUT */) const {
  if (std::find(formats.begin(), formats.end(), BROTLI_SHARED_DICT) !=
      formats.end()) {
    // Brotli is preferred, so always pick it, if it's accepted by the client.
    format = BROTLI_SHARED_DICT;
    return brotli_binary_diff_.get();
  }
  if (std::find(formats.begin(), formats.end(), VCDIFF) != formats.end()) {
    format = VCDIFF;
    return vcdiff_binary_diff_.get();
  }

  return nullptr;
}

}  // namespace patch_subset
