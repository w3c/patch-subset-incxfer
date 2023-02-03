#ifndef PATCH_SUBSET_CODEPOINT_MAP_H_
#define PATCH_SUBSET_CODEPOINT_MAP_H_

#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "hb.h"

namespace patch_subset {

/*
 * Stores a reversible mapping for codepoints values.
 *
 * A mapping can be used to transform codepoint values into a
 * new space (for example {0x41, 0x45, 0x61} -> {0, 1, 2}).
 *
 * Encode is used to transform original codepoint values into
 * their new values. While Decode undoes this transformation.
 *
 * Can serialize the mapping into a CBOR representation and
 * load a mapping from a previously serialized mapping.
 */
class CodepointMap {
 public:
  void Clear();

  // Adds a mapping that transforms codepoint 'from' into
  // the value 'to'.
  void AddMapping(hb_codepoint_t from, hb_codepoint_t to);

  // Load the codepoint remapping specified in vector of ints. Replaces
  // any existing mappings currently in this object.
  void FromVector(const std::vector<int32_t>& ints);

  // Serialize this mapping to a vector of ints.
  absl::Status ToVector(std::vector<int32_t>* ints) const;

  // Apply the mapping transformation to all codepoints in the provided set.
  // All values in the 'codepoints' set are replaced with the transformed
  // values.
  absl::Status Encode(hb_set_t* codepoints /* IN/OUT */) const;

  // Transform cp using the this mapping. Writes the transformed value
  // to cp.
  absl::Status Encode(hb_codepoint_t* cp /* IN/OUT */) const;

  // Restores a set of encoded codepoints to their original values.
  // All values in the 'codepoints' set are replaced with their decoded
  // values.
  absl::Status Decode(hb_set_t* codepoints) const;

  // Restore encoded cp to it's original value.
  absl::Status Decode(hb_codepoint_t* cp /* IN/OUT */) const;

  // Given a set of untransformed codepoints, intersects it
  // with the set of codepoints that this mapping can map.
  void IntersectWithMappedCodepoints(hb_set_t* codepoints) const;

 private:
  std::unordered_map<hb_codepoint_t, hb_codepoint_t> encode_map;
  std::unordered_map<hb_codepoint_t, hb_codepoint_t> decode_map;
};

}  // namespace patch_subset

#endif  // PATCH_SUBSET_CODEPOINT_MAP_H_
