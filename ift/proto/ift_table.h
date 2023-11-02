#ifndef IFT_PROTO_IFT_TABLE_H_
#define IFT_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/patch_map.h"
#include "patch_subset/font_data.h"

namespace ift::proto {

/*
 * Abstract representation of a IFT table. Used to load, construct, and/or
 * modify IFT tables in fonts.
 */
class IFTTable {
 public:
  /*
   * Returns the IFT table found in 'face'.
   */
  static absl::StatusOr<IFTTable> FromFont(hb_face_t* face);

  /*
   * Returns the IFT table found in 'font'.
   */
  static absl::StatusOr<IFTTable> FromFont(const patch_subset::FontData& font);

  /*
   * Converts IFT proto into an IFTTable object.
   */
  static absl::StatusOr<IFTTable> FromProto(IFT proto);

  /*
   * Adds an encoded 'IFT ' table built from the supplied proto to font pointed
   * to by face. By default this will maintain the physical ordering of tables
   * already present in the font. If iftb_conversion is set any "IFTB" tables
   * if present will be be removed and tables in the final font will be ordered
   * according to IFTB ordering requirements.
   */
  static absl::StatusOr<patch_subset::FontData> AddToFont(
      hb_face_t* face, const IFT& proto, const IFT* extension_proto = nullptr,
      bool iftb_conversion = false);

  void GetId(uint32_t out[4]) const;

  const PatchMap& GetPatchMap() const { return patch_map_; }
  PatchMap& GetPatchMap() { return patch_map_; }
  bool HasExtensionEntries() const;

  const std::string& GetUrlTemplate() const { return url_template_; }

  /*
   * Adds a copy of this table to the supplied 'face'.
   */
  absl::StatusOr<patch_subset::FontData> AddToFont(hb_face_t* face);

 private:
  std::string url_template_;
  uint32_t id_[4];
  PatchEncoding default_encoding_;
  PatchMap patch_map_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_IFT_TABLE_H_