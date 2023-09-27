#include "patch_subset/proto/ift_table.h"

#include <google/protobuf/text_format.h>

#include "absl/strings/string_view.h"
#include "patch_subset/proto/IFT.pb.h"

namespace patch_subset::proto {

absl::StatusOr<IFTTable> IFTTable::FromFont(hb_face_t* face) {
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

  // TODO: return ift;
  return absl::InternalError("Unable to parse 'IFT ' table.");
}

}  // namespace patch_subset::proto