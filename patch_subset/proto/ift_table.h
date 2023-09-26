#ifndef PATCH_SUBSET_PROTO_IFT_TABLE_H_
#define PATCH_SUBSET_PROTO_IFT_TABLE_H_

#include "absl/status/statusor.h"
#include "hb.h"
#include "patch_subset/proto/IFT.pb.h"

namespace patch_subset::proto {

class IFTTable {
  static absl::StatusOr<IFTTable> FromFont(hb_face_t* face);

  explicit IFTTable(IFT ift_proto) : ift_proto_(ift_proto) {}

 private:
  IFT ift_proto_;
};

}  // namespace patch_subset::proto

#endif  // PATCH_SUBSET_PATCH_SUBSET_CLIENT_H_