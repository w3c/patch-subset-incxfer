#ifndef IFT_PROTO_IFT_TABLE_H_
#define IFT_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"

namespace ift::proto {

typedef absl::flat_hash_map<uint32_t, std::pair<uint32_t, PatchEncoding>>
    patch_map;

class IFTTable {
 public:
  static absl::StatusOr<IFTTable> FromFont(hb_face_t* face);
  static absl::StatusOr<IFTTable> FromProto(IFT proto);
  static absl::StatusOr<patch_subset::FontData> AddToFont(hb_face_t* face,
                                                          IFT proto);

  const patch_map& get_patch_map() const;

  std::string patch_to_url(uint32_t patch_idx) const;

 private:
  explicit IFTTable(IFT ift_proto, patch_map patch_map)
      : patch_map_(patch_map), ift_proto_(ift_proto) {}

  static absl::StatusOr<patch_map> create_patch_map(const IFT& ift_proto);

  patch_map patch_map_;
  IFT ift_proto_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_IFT_TABLE_H_