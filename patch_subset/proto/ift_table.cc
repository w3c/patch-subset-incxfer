#include "patch_subset/proto/ift_table.h"

#include <google/protobuf/text_format.h>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using absl::StatusOr;
using patch_subset::SparseBitSet;

namespace patch_subset::proto {

StatusOr<IFTTable> IFTTable::FromFont(hb_face_t* face) {
  hb_blob_t* ift_table =
      hb_face_reference_table(face, HB_TAG('I', 'F', 'T', ' '));
  if (ift_table == hb_blob_get_empty()) {
    return absl::InvalidArgumentError("'IFT ' table not found in face.");
  }

  unsigned length;
  const char* data = hb_blob_get_data(ift_table, &length);
  std::string data_string(data, length);
  hb_blob_destroy(ift_table);

  IFT ift;
  if (!ift.ParseFromString(data_string)) {
    return absl::InternalError("Unable to parse 'IFT ' table.");
  }

  auto m = create_patch_map(ift);
  if (!m.ok()) {
    return m.status();
  }

  return IFTTable(ift, *m);
}

std::string IFTTable::chunk_to_url(uint32_t patch_idx) const {
  // TODO(garretrieger): implement me.
  return "NOT_IMPLEMENTED";
}

const flat_hash_map<uint32_t, uint32_t>& IFTTable::get_patch_map() const {
  return patch_map_;
}

StatusOr<flat_hash_map<uint32_t, uint32_t>> IFTTable::create_patch_map(
    const IFT& ift) {
  flat_hash_map<uint32_t, uint32_t> result;
  for (auto m : ift.subset_mapping()) {
    uint32_t bias = m.bias();
    uint32_t patch_idx = m.id();

    hb_set_unique_ptr codepoints = make_hb_set();
    auto s = SparseBitSet::Decode(m.codepoint_set(), codepoints.get());
    if (!s.ok()) {
      return s;
    }

    hb_codepoint_t cp = HB_SET_VALUE_INVALID;
    while (hb_set_next(codepoints.get(), &cp)) {
      // TODO(garretrieger): currently we assume that a codepoints maps to only
      // one chunk,
      //   however, this is not always going to be true. Chunk selection needs
      //   to be more complicated then a simple map.
      uint32_t actual_cp = cp + bias;
      if (result.contains(actual_cp)) {
        return absl::InvalidArgumentError(
            "cannot load IFT table that maps a codepoint to more than one "
            "patch.");
      }
      result[actual_cp] = patch_idx;
    }
  }

  return result;
}

}  // namespace patch_subset::proto