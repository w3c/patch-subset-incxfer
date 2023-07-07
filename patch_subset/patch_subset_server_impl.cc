#include "patch_subset/patch_subset_server_impl.h"

#include <stdio.h>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "hb-ot.h"
#include "hb-subset.h"
#include "hb.h"
#include "patch_subset/cbor/axis_space.h"
#include "patch_subset/cbor/client_state.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using patch_subset::cbor::AxisInterval;
using patch_subset::cbor::AxisSpace;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
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
        encoding(Encodings::kIdentityEncoding) {}

  bool IsPatch() const {
    return !IsFallback() && !hb_set_is_empty(codepoints_have.get());
  }

  bool IsRebase() const { return !IsFallback() && !IsPatch(); }

  bool IsFallback() const { return codepoint_mapping_invalid; }

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
  std::string encoding;
};

Status PatchSubsetServerImpl::Handle(
    const std::string& font_id, const std::vector<std::string>& accept_encoding,
    const PatchRequest& request, FontData& response,
    std::string& content_encoding) {
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

  if (state.IsFallback()) {
    return ConstructResponse(state, response, content_encoding);
  }

  if (!(result = ComputeSubsets(font_id, state)).ok()) {
    return result;
  }

  ValidatePatchBase(request.BaseChecksum(), &state);

  const BinaryDiff* binary_diff = DiffFor(accept_encoding, state.IsPatch(), state.encoding);
  if (!binary_diff) {
    return absl::InvalidArgumentError(
        "No available binary diff algorithms were specified.");
  }
  if (!(result = binary_diff->Diff(state.client_subset,
                                   state.client_target_subset, &state.patch))
           .ok()) {
    return result;
  }

  // TODO(garretrieger): handle exceptional cases (see design doc).

  return ConstructResponse(state, response, content_encoding);
}

Status PatchSubsetServerImpl::LoadInputCodepoints(const PatchRequest& request,
                                                  RequestState* state) const {
  Status result;
  result.Update(CompressedSet::Decode(request.CodepointsHave(),
                                      state->codepoints_have.get()));
  result.Update(CompressedSet::Decode(request.CodepointsNeeded(),
                                      state->codepoints_needed.get()));
  result.Update(
      CompressedSet::Decode(request.IndicesHave(), state->indices_have.get()));
  result.Update(CompressedSet::Decode(request.IndicesNeeded(),
                                      state->indices_needed.get()));

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
                                             RequestState& state) const {
  ClientState client_state;
  Status result = CreateClientState(state, client_state);
  if (!result.ok()) {
    return result;
  }

  std::string client_state_table;
  if (!(result = client_state.SerializeToString(client_state_table)).ok()) {
    return result;
  }

  result = subsetter_->Subset(state.font_data, *state.codepoints_have,
                              client_state_table, &state.client_subset);
  if (!result.ok()) {
    LOG(WARNING) << "Subsetting for client_subset "
                 << "(font_id = " << font_id << ")"
                 << "failed.";
    return result;
  }

  result = subsetter_->Subset(state.font_data, *state.codepoints_needed,
                              client_state_table, &state.client_target_subset);
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

Status PatchSubsetServerImpl::ConstructResponse(
    const RequestState& state, FontData& response,
    std::string& content_encoding) const {
  if (state.IsFallback()) {
    // Just send back the whole font.
    // TODO(garretrieger): do a regular brotli compression.
    content_encoding = Encodings::kIdentityEncoding;
    response.shallow_copy(state.font_data);
    return absl::OkStatus();
  }

  content_encoding = state.encoding;
  response.shallow_copy(state.patch);

  return absl::OkStatus();
}

Status PatchSubsetServerImpl::ValidateChecksum(uint64_t checksum,
                                               const FontData& data) const {
  uint64_t actual_checksum = hasher_->Checksum(data.str());
  if (actual_checksum != checksum) {
    return absl::InvalidArgumentError(
        absl::StrCat("Checksum mismatch. Expected = ", checksum,
                     " Actual = ", actual_checksum, "."));
  }
  return absl::OkStatus();
}

void PatchSubsetServerImpl::AddVariableAxesData(
    const FontData& font_data, ClientState& client_state) const {
  hb_face_t* face = font_data.reference_face();

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

  client_state.SetSubsetAxisSpace(space);
  client_state.SetOriginalAxisSpace(space);

  hb_face_destroy(face);
}

Status PatchSubsetServerImpl::CreateClientState(
    const RequestState& state, ClientState& client_state) const {
  client_state.SetOriginalFontChecksum(
      hasher_->Checksum(string_view(state.font_data.str())));

  if (codepoint_mapper_) {
    vector<int32_t> ordering;
    Status result = state.mapping.ToVector(&ordering);
    if (!result.ok()) {
      return result;
    }
    client_state.SetCodepointOrdering(ordering);
  }

  AddVariableAxesData(state.font_data, client_state);

  return absl::OkStatus();
}

const BinaryDiff* PatchSubsetServerImpl::DiffFor(
    const std::vector<std::string>& accept_encoding,
    bool is_patch,
    std::string& encoding /* OUT */) const {
  if (!is_patch
      && std::find(accept_encoding.begin(), accept_encoding.end(),
                   Encodings::kBrotliEncoding) != accept_encoding.end()) {
    // Brotli is preferred and this is not a patch, so just use regular brotli.
    encoding = Encodings::kBrotliEncoding;
    return brotli_binary_diff_.get();
  }

  if (std::find(accept_encoding.begin(), accept_encoding.end(),
                Encodings::kBrotliDiffEncoding) != accept_encoding.end()) {
    // Brotli is preferred, so always pick it, if it's accepted by the client.
    encoding = Encodings::kBrotliDiffEncoding;
    return brotli_binary_diff_.get();
  }

  if (std::find(accept_encoding.begin(), accept_encoding.end(),
                Encodings::kVCDIFFEncoding) != accept_encoding.end()) {
    encoding = Encodings::kVCDIFFEncoding;
    return vcdiff_binary_diff_.get();
  }

  // TODO(garretrieger): fallback to br or gzip if patching is not supported.
  // TODO(garretrieger): use br or gzip if rebasing and sbr is not supported
  // (instead of VCDIFF).
  return nullptr;
}

}  // namespace patch_subset
