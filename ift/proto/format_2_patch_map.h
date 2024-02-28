#ifndef IFT_PROTO_FORMAT_2_PATCH_MAP_H_
#define IFT_PROTO_FORMAT_2_PATCH_MAP_H_

#include "absl/status/statusor.h"
#include "ift/proto/ift_table.h"

namespace ift::proto {

class Format2PatchMap {
 public:
  static absl::Status Deserialize(absl::string_view data, IFTTable& out,
                                  bool is_ext);

  static absl::StatusOr<std::string> Serialize(const IFTTable& ift_table,
                                               bool is_ext);
};

}  // namespace ift::proto

#endif  // IFT_PROTO_FORMAT_2_PATCH_MAP_H_
