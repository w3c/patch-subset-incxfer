#ifndef IFT_PROTO_FORMAT_2_PATCH_MAP_H_
#define IFT_PROTO_FORMAT_2_PATCH_MAP_H_

#include "absl/status/statusor.h"
#include "ift/proto/patch_map.h"

namespace ift::proto {

class Format2PatchMap {
 public:
  static absl::Status Deserialize(absl::string_view data, PatchMap& out,
                                  std::string& uri_template_out);
  static absl::StatusOr<std::string> Serialize(const PatchMap& patch_map,
                                               bool is_ext,
                                               absl::string_view uri_template);
};

}  // namespace ift::proto

#endif  // IFT_PROTO_FORMAT_2_PATCH_MAP_H_
