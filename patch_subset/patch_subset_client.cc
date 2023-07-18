#include "patch_subset/patch_subset_client.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::Status;
using absl::StatusOr;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;

StatusOr<PatchRequest> PatchSubsetClient::CreateRequest(
    const hb_set_t& additional_codepoints, const FontData& font_subset) const {
  hb_face_t* subset_face = font_subset.reference_face();

  hb_set_unique_ptr existing_codepoints = make_hb_set();
  hb_face_collect_unicodes(subset_face, existing_codepoints.get());

  StatusOr<ClientState> client_state = ClientState();
  if (!font_subset.empty()) {
    client_state = ClientState::FromFont(subset_face);
    if (!client_state.ok()) {
      return client_state.status();
    }
  }

  hb_face_destroy(subset_face);

  hb_set_unique_ptr new_codepoints = make_hb_set();
  hb_set_union(new_codepoints.get(), &additional_codepoints);
  hb_set_subtract(new_codepoints.get(), existing_codepoints.get());

  Status result = EncodeCodepoints(*client_state, existing_codepoints.get(),
                                   new_codepoints.get());
  if (!result.ok()) {
    return result;
  }

  if (!hb_set_get_population(new_codepoints.get())) {
    // No new codepoints are needed. No action needed.
    return PatchRequest();
  }

  uint64_t base_checksum = hasher_->Checksum(font_subset.str());
  return CreateRequest(*existing_codepoints, *new_codepoints, base_checksum,
                       *client_state);
}

StatusOr<FontData> PatchSubsetClient::DecodeResponse(
    const FontData& font_subset, const FontData& encoded_response,
    const std::string& encoding) const {
  if (encoding == Encodings::kIdentityEncoding) {
    FontData copy;
    copy.shallow_copy(encoded_response);
    return copy;
  }

  if (encoding != Encodings::kBrotliDiffEncoding &&
      encoding != Encodings::kBrotliEncoding) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported patch encoding ", encoding));
  }

  FontData patched;
  Status s = binary_patch_->Patch(font_subset, encoded_response, &patched);
  if (!s.ok()) {
    return s;
  }

  return patched;
}

Status PatchSubsetClient::EncodeCodepoints(const ClientState& state,
                                           hb_set_t* codepoints_have,
                                           hb_set_t* codepoints_needed) const {
  if (state.CodepointOrdering().empty()) {
    return absl::OkStatus();
  }

  CodepointMap map;
  map.FromVector(state.CodepointOrdering());

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

PatchRequest PatchSubsetClient::CreateRequest(const hb_set_t& codepoints_have,
                                              const hb_set_t& codepoints_needed,
                                              uint64_t base_checksum,
                                              const ClientState& state) const {
  PatchRequest request;

  if (hb_set_get_population(&codepoints_have)) {
    request.SetOriginalFontChecksum(state.OriginalFontChecksum());
    request.SetBaseChecksum(base_checksum);
  }

  if (!hb_set_is_empty(&codepoints_have)) {
    patch_subset::cbor::CompressedSet codepoints_have_encoded;
    CompressedSet::Encode(codepoints_have, codepoints_have_encoded);

    if (state.CodepointOrdering().empty()) {
      request.SetCodepointsHave(codepoints_have_encoded);
    } else {
      request.SetIndicesHave(codepoints_have_encoded);
    }
  }

  if (!hb_set_is_empty(&codepoints_needed)) {
    patch_subset::cbor::CompressedSet codepoints_needed_encoded;
    CompressedSet::Encode(codepoints_needed, codepoints_needed_encoded);

    if (state.CodepointOrdering().empty()) {
      request.SetCodepointsNeeded(codepoints_needed_encoded);
    } else {
      request.SetIndicesNeeded(codepoints_needed_encoded);
    }
  }

  if (!state.CodepointOrdering().empty()) {
    request.SetOrderingChecksum(
        ordering_hasher_->Checksum(state.CodepointOrdering()));
  }

  return request;
}

}  // namespace patch_subset
