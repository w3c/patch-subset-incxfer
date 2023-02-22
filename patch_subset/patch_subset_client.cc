#include "patch_subset/patch_subset_client.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "hb.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/integer_list_checksum_impl.h"

namespace patch_subset {

using absl::Status;
using absl::StatusOr;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;

hb_face_t* ToFace(const FontData& font_subset) {
  hb_blob_t* blob = hb_blob_create(font_subset.data(), font_subset.size(),
                                   HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  return face;
}

StatusOr<PatchRequest> PatchSubsetClient::CreateRequest(
    const hb_set_t& additional_codepoints, const FontData& font_subset) const {
  hb_face_t* subset_face = ToFace(font_subset);

  hb_set_unique_ptr existing_codepoints = make_hb_set();
  hb_face_collect_unicodes(subset_face, existing_codepoints.get());

  auto client_state = GetStateTable(subset_face);
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

  return CreateRequest(*existing_codepoints, *new_codepoints, font_subset,
                       *client_state);
}

StatusOr<FontData> PatchSubsetClient::DecodeResponse(
    const FontData& font_subset, const FontData& encoded_response,
    const std::string& encoding) const {
  if (encoding != Encodings::kBrotliDiffEncoding) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unsupported patch encoding ", encoding));
  }

  FontData patched;
  Status s = binary_patch_->Patch(font_subset, encoded_response, &patched);
  if (!s.ok()) {
    return s;
  }

  return patched;
}

StatusOr<ClientState> PatchSubsetClient::GetStateTable(
    const hb_face_t* face) const {
  hb_blob_t* state_table =
      hb_face_reference_table(face, HB_TAG('I', 'F', 'T', 'P'));
  if (state_table == hb_blob_get_empty()) {
    return absl::InvalidArgumentError("IFTP table not found in face.");
  }

  unsigned length;
  const char* data = hb_blob_get_data(state_table, &length);
  std::string data_string(data, length);
  hb_blob_destroy(state_table);

  ClientState state;
  Status s = ClientState::ParseFromString(data_string, state);
  if (s.ok()) {
    return state;
  }
  return s;
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
                                              const FontData& font_subset,
                                              const ClientState& state) const {
  PatchRequest request;

  if (hb_set_get_population(&codepoints_have)) {
    request.SetOriginalFontChecksum(state.OriginalFontChecksum());
    request.SetBaseChecksum(hasher_->Checksum(font_subset.str()));
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
    IntegerListChecksumImpl checksum(hasher_.get());
    request.SetOrderingChecksum(checksum.Checksum(state.CodepointOrdering()));
  }

  return request;
}

}  // namespace patch_subset
