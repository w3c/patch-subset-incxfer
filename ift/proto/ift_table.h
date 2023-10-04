#ifndef IFT_PROTO_IFT_TABLE_H_
#define IFT_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
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
  static absl::StatusOr<IFTTable> FromFont(const patch_subset::FontData& font);
  static absl::StatusOr<IFTTable> FromProto(IFT proto);

  // Adds an encoded 'IFT ' table built from the supplied proto to font pointed
  // to by face. By default this will maintain the physical ordering of tables
  // already present in the font. If iftb_conversion is set any "IFTB" tables
  // if present will be be removed and tables in the final font will be ordered
  // according to IFTB ordering requirements.
  static absl::StatusOr<patch_subset::FontData> AddToFont(
      hb_face_t* face, const IFT& proto, bool iftb_conversion = false);

  void GetId(uint32_t out[4]) const;
  const patch_map& GetPatchMap() const;
  std::string PatchToUrl(uint32_t patch_idx) const;

  absl::Status RemovePatches(const absl::flat_hash_set<uint32_t> patch_indices);
  absl::StatusOr<patch_subset::FontData> AddToFont(hb_face_t* face);

 private:
  explicit IFTTable(IFT ift_proto, patch_map patch_map)
      : patch_map_(patch_map), ift_proto_(ift_proto) {
    for (int i = 0; i < 4; i++) {
      if (i < ift_proto_.id_size()) {
        id_[i] = ift_proto_.id(i);
      } else {
        id_[i] = 0;
      }
    }
  }

  static absl::StatusOr<patch_map> CreatePatchMap(const IFT& ift_proto);

  patch_map patch_map_;
  IFT ift_proto_;
  uint32_t id_[4];
};

}  // namespace ift::proto

#endif  // IFT_PROTO_IFT_TABLE_H_