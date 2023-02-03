#include "patch_subset/patch_subset_server_impl.h"

#include <stdio.h>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "hb-ot.h"
#include "hb-subset.h"
#include "hb.h"
#include "patch_subset/cbor/axis_space.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using patch_subset::cbor::AxisInterval;
using patch_subset::cbor::AxisSpace;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;
using std::vector;

// Helper object, which holds all of the relevant state for
// handling a single request.
struct RequestState {
  RequestState()
      : codepoints_have(make_hb_set()),
        codepoints_needed(make_hb_set()),
        indices_have(make_hb_set()),
        indices_needed(make_hb_set()),
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
  hb_set_unique_ptr indices_have;
  hb_set_unique_ptr indices_needed;

  uint64_t ordering_checksum;
  CodepointMap mapping;
  FontData font_data;
  FontData client_subset;
  FontData client_target_subset;
  FontData patch;
  bool codepoint_mapping_invalid;
  PatchFormat format;
};

Status PatchSubsetServerImpl::Handle(const std::string& font_id,
                                     const PatchRequest& request,
                                     PatchResponse& response) {
  RequestState state;

  Status result = LoadInputCodepoints(request, &state);
  if (!result.ok()) {
    return result;
  }

  if (!RequiredFieldsPresent(request, state)) {
    return absl::InvalidArgumentError("Request is missing required fields.");
  }

  result = font_provider_->GetFont(font_id, &state.font_data);
  if (!result.ok()) {
    return result;
  }

  CheckOriginalChecksum(request.OriginalFontChecksum(), &state);

  if (codepoint_mapper_) {
    if (!(result = ComputeCodepointRemapping(&state)).ok()) {
      return result;
    }
  }

  AddPredictedCodepoints(&state);

  if (state.IsReindex()) {
    result = ConstructResponse(state, response);
    if (!result.ok()) {
      return result;
    }
    return absl::OkStatus();
  }

  if (!(result = ComputeSubsets(font_id, &state)).ok()) {
    return result;
  }

  ValidatePatchBase(request.BaseChecksum(), &state);

  const BinaryDiff* binary_diff =
      DiffFor(request.AcceptFormats(), state.format);
  if (!binary_diff) {
    return absl::InvalidArgumentError("No available binary diff algorithms were specified.");
  }
  if (!(result = binary_diff->Diff(
                 state.client_subset, state.client_target_subset, &state.patch)).ok()) {
    return result;
  }

  // TODO(garretrieger): handle exceptional cases (see design doc).

  return ConstructResponse(state, response);
}

Status PatchSubsetServerImpl::LoadInputCodepoints(const PatchRequest& request,
                                                  RequestState* state) const {
  Status result;
  result.Update(CompressedSet::Decode(request.CodepointsHave(), state->codepoints_have.get()));
  result.Update(CompressedSet::Decode(request.CodepointsNeeded(),
                                      state->codepoints_needed.get()));
  result.Update(CompressedSet::Decode(request.IndicesHave(), state->indices_have.get()));
  result.Update(CompressedSet::Decode(request.IndicesNeeded(), state->indices_needed.get()));

  if (!result.ok()) {
    return result;
  }

  hb_set_union(state->codepoints_needed.get(), state->codepoints_have.get());
  hb_set_union(state->indices_needed.get(), state->indices_have.get());
  state->ordering_checksum = request.OrderingChecksum();

  return absl::OkStatus();
}

bool PatchSubsetServerImpl::RequiredFieldsPresent(
    const PatchRequest& request, const RequestState& state) const {
  if ((!hb_set_is_empty(state.codepoints_have.get()) ||
       !hb_set_is_empty(state.indices_have.get())) &&
      (!request.HasBaseChecksum() || !request.HasOriginalFontChecksum())) {
    LOG(WARNING) << "Request has indicated it has existing codepoints but does "
                 << "not set a base and/or original checksum.";
    return false;
  }

  if ((!hb_set_is_empty(state.indices_have.get()) ||
       !hb_set_is_empty(state.indices_needed.get())) &&
      !request.HasOrderingChecksum()) {
    LOG(WARNING) << "Request requires a codepoint remapping but does not "
                 << "provide an ordering checksum.";
    return false;
  }

  if (!request.HasProtocolVersion() ||
      request.GetProtocolVersion() != ProtocolVersion::ONE) {
    LOG(WARNING) << "Protocol version must be 0.";
    return false;
  }

  return true;
}

void PatchSubsetServerImpl::CheckOriginalChecksum(uint64_t original_checksum,
                                                  RequestState* state) const {
  if (state->IsPatch() &&
      !ValidateChecksum(original_checksum, state->font_data).ok()) {
    LOG(WARNING) << "Client's original checksum does not match. Switching to "
        "REBASE.";
    hb_set_clear(state->codepoints_have.get());
  }
}

Status PatchSubsetServerImpl::ComputeCodepointRemapping(
    RequestState* state) const {
  hb_set_unique_ptr codepoints = make_hb_set();
  subsetter_->CodepointsInFont(state->font_data, codepoints.get());
  codepoint_mapper_->ComputeMapping(codepoints.get(), &state->mapping);

  if (hb_set_is_empty(state->indices_have.get()) &&
      hb_set_is_empty(state->indices_needed.get())) {
    // Don't remap input codepoints if none are specified as indices.
    return absl::OkStatus();
  }

  vector<int32_t> mapping_ints;
  Status result = state->mapping.ToVector(&mapping_ints);
  if (!result.ok()) {
    // This typically shouldn't happen, so bail with internal error.
    return result;
  }

  uint64_t expected_checksum = integer_list_checksum_->Checksum(mapping_ints);
  if (expected_checksum != state->ordering_checksum) {
    LOG(WARNING) << "Client ordering checksum (" << state->ordering_checksum
                 << ") does not match expected checksum (" << expected_checksum
                 << "). Sending a REINDEX response.";
    state->codepoint_mapping_invalid = true;
    return absl::OkStatus();
  }

  // Codepoints given to use by the client are using the computed codepoint
  // mapping, so translate the provided sets back to actual codepoints.
  result = state->mapping.Decode(state->indices_have.get());
  result.Update(state->mapping.Decode(state->indices_needed.get()));
  if (!result.ok()) {
    return result;
  }

  hb_set_union(state->codepoints_have.get(), state->indices_have.get());
  hb_set_union(state->codepoints_needed.get(), state->indices_needed.get());
  return absl::OkStatus();
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

Status PatchSubsetServerImpl::ComputeSubsets(const std::string& font_id,
                                                 RequestState* state) const {
  Status result = subsetter_->Subset(
      state->font_data, *state->codepoints_have, &state->client_subset);
  if (!result.ok()) {
    LOG(WARNING) << "Subsetting for client_subset "
                 << "(font_id = " << font_id << ")"
                 << "failed.";
    return result;
  }

  result = subsetter_->Subset(state->font_data, *state->codepoints_needed,
                              &state->client_target_subset);
  if (!result.ok()) {
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
      !ValidateChecksum(base_checksum, state->client_subset).ok()) {
    LOG(WARNING) << "Client's base does not match. Switching to REBASE.";
    // Clear the client_subset since it doesn't match. The diff will then diff
    // in rebase mode.
    state->client_subset.reset();
    hb_set_clear(state->codepoints_have.get());
  }
}

Status PatchSubsetServerImpl::ConstructResponse(const RequestState& state,
                                                PatchResponse& response) const {
  response.SetProtocolVersion(ProtocolVersion::ONE);
  if ((state.IsReindex() || state.IsRebase()) && codepoint_mapper_) {
    vector<int32_t> ordering;
    Status result = state.mapping.ToVector(&ordering);
    if (!result.ok()) {
      return result;
    }
    response.SetCodepointOrdering(ordering);
    response.SetOrderingChecksum(integer_list_checksum_->Checksum(ordering));
  }

  if (state.IsReindex()) {
    AddChecksums(state.font_data, response);
    // Early return, no patch is needed for a re-index.
    return absl::OkStatus();
  }

  response.SetPatchFormat(state.format);
  if (state.IsPatch()) {
    response.SetPatch(state.patch.string());
  } else if (state.IsRebase()) {
    response.SetReplacement(state.patch.string());
  }

  AddChecksums(state.font_data, state.client_target_subset, response);
  AddVariableAxesData(state.font_data, response);

  return absl::OkStatus();
}

Status PatchSubsetServerImpl::ValidateChecksum(uint64_t checksum,
                                                   const FontData& data) const {
  uint64_t actual_checksum = hasher_->Checksum(data.str());
  if (actual_checksum != checksum) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Checksum mismatch. Expected = ", checksum,
        " Actual = ", actual_checksum, "."));
  }
  return absl::OkStatus();
}

void PatchSubsetServerImpl::AddVariableAxesData(const FontData& font_data,
                                                PatchResponse& response) const {
  hb_blob_t* blob = font_data.reference_blob();
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  if (!hb_ot_var_has_data(face)) {
    // No variable axes.
    hb_face_destroy(face);
    return;
  }

  AxisSpace space;
  hb_ot_var_axis_info_t axes[10];
  unsigned offset = 0, num_axes = 10;
  unsigned total_axes = hb_ot_var_get_axis_count(face);
  while (offset < total_axes) {
    hb_ot_var_get_axis_infos(face, offset, &num_axes, axes);

    for (unsigned i = 0; i < num_axes; i++) {
      space.AddInterval(axes[i].tag,
                        AxisInterval(axes[i].min_value, axes[i].max_value));
    }

    offset += num_axes;
  }

  response.SetSubsetAxisSpace(space);
  response.SetOriginalAxisSpace(space);

  hb_face_destroy(face);
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
