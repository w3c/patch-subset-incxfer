#include "patch_subset/codepoint_map.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "common/hb_set_unique_ptr.h"
#include "hb.h"

namespace patch_subset {

using absl::Status;
using common::hb_set_unique_ptr;
using common::make_hb_set;

void CodepointMap::Clear() {
  encode_map.clear();
  decode_map.clear();
}

void CodepointMap::AddMapping(hb_codepoint_t from, hb_codepoint_t to) {
  encode_map[from] = to;
  decode_map[to] = from;
}

void CodepointMap::FromVector(const std::vector<int32_t>& ints) {
  Clear();
  int index = 0;
  for (int32_t cp : ints) {
    AddMapping(cp, index++);
  }
}

Status CodepointMap::ToVector(std::vector<int32_t>* ints) const {
  ints->resize(encode_map.size());
  ints->clear();
  for (unsigned int i = 0; i < encode_map.size(); i++) {
    hb_codepoint_t cp_for_index = i;
    Status result = Decode(&cp_for_index);
    if (!result.ok()) {
      return result;
    }
    ints->push_back(cp_for_index);
  }
  return absl::OkStatus();
}

Status ApplyMappingTo(
    const std::unordered_map<hb_codepoint_t, hb_codepoint_t>& mapping,
    hb_codepoint_t* cp) {
  auto new_cp = mapping.find(*cp);
  if (new_cp == mapping.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Encountered codepoint that is unspecified in the remapping: ", *cp));
  }

  *cp = new_cp->second;
  return absl::OkStatus();
}

Status ApplyMappingTo(
    const std::unordered_map<hb_codepoint_t, hb_codepoint_t>& mapping,
    hb_set_t* codepoints) {
  hb_set_unique_ptr new_codepoints = make_hb_set();

  for (hb_codepoint_t cp = HB_SET_VALUE_INVALID;
       hb_set_next(codepoints, &cp);) {
    hb_codepoint_t new_cp = cp;
    Status result = ApplyMappingTo(mapping, &new_cp);
    if (!result.ok()) {
      return result;
    }
    hb_set_add(new_codepoints.get(), new_cp);
  }

  hb_set_clear(codepoints);
  hb_set_union(codepoints, new_codepoints.get());

  return absl::OkStatus();
}

Status CodepointMap::Encode(hb_set_t* codepoints) const {
  return ApplyMappingTo(encode_map, codepoints);
}

Status CodepointMap::Encode(hb_codepoint_t* cp) const {
  return ApplyMappingTo(encode_map, cp);
}

Status CodepointMap::Decode(hb_set_t* codepoints) const {
  return ApplyMappingTo(decode_map, codepoints);
}

Status CodepointMap::Decode(hb_codepoint_t* cp) const {
  return ApplyMappingTo(decode_map, cp);
}

void CodepointMap::IntersectWithMappedCodepoints(hb_set_t* codepoints) const {
  hb_set_unique_ptr mapped_codepoints = make_hb_set();
  for (auto entry : encode_map) {
    hb_set_add(mapped_codepoints.get(), entry.first);
  }

  hb_set_intersect(codepoints, mapped_codepoints.get());
}

}  // namespace patch_subset
