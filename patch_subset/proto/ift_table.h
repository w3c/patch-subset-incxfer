#ifndef PATCH_SUBSET_PROTO_IFT_TABLE_H_
#define PATCH_SUBSET_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/proto/IFT.pb.h"

namespace patch_subset::proto {

class IFTTable {
 public:
  static absl::StatusOr<IFTTable> FromFont(hb_face_t* face);
  static absl::StatusOr<IFTTable> FromProto(IFT proto);
  static absl::StatusOr<FontData> AddToFont(hb_face_t* face, IFT proto);

  const absl::flat_hash_map<uint32_t, uint32_t>& get_patch_map() const;

  std::string patch_to_url(uint32_t patch_idx) const;

 private:
  explicit IFTTable(IFT ift_proto,
                    absl::flat_hash_map<uint32_t, uint32_t> patch_map)
      : patch_map_(patch_map), ift_proto_(ift_proto) {}

  static absl::StatusOr<absl::flat_hash_map<uint32_t, uint32_t>>
  create_patch_map(const IFT& ift_proto);

  absl::flat_hash_map<uint32_t, uint32_t> patch_map_;
  IFT ift_proto_;
};

}  // namespace patch_subset::proto

#endif  // PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_