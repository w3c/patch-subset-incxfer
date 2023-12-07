#ifndef IFT_PROTO_IFT_TABLE_H_
#define IFT_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "common/font_data.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/patch_map.h"

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
  static absl::StatusOr<IFTTable> FromFont(const common::FontData& font);

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
  static absl::StatusOr<common::FontData> AddToFont(
      hb_face_t* face, const IFT& proto, const IFT* extension_proto = nullptr,
      bool iftb_conversion = false);

  void GetId(uint32_t out[4]) const;

  const PatchMap& GetPatchMap() const { return patch_map_; }
  PatchMap& GetPatchMap() { return patch_map_; }
  bool HasExtensionEntries() const;

  const std::string& GetUrlTemplate() const { return url_template_; }
  const std::string& GetExtensionUrlTemplate() const {
    return extension_url_template_;
  }
  void SetUrlTemplate(absl::string_view value) {
    url_template_ = value;
    extension_url_template_ = value;
  }
  void SetUrlTemplate(absl::string_view value,
                      absl::string_view extension_value) {
    url_template_ = value;
    extension_url_template_ = extension_value;
  }
  void SetExtensionUrlTemplate(absl::string_view value) {
    extension_url_template_ = value;
  }

  absl::Status SetId(absl::Span<const uint32_t> id) {
    if (id.size() != 4) {
      return absl::InvalidArgumentError("ID must have length of 4.");
    }
    id_[0] = id[0];
    id_[1] = id[1];
    id_[2] = id[2];
    id_[3] = id[3];
    return absl::OkStatus();
  }

  /*
   * Converts this abstract representation to the proto representation.
   * This method generates the proto for the main "IFT " table.
   */
  IFT CreateMainTable() const;

  /*
   * Converts this abstract representation to the proto representation.
   * This method generates the proto for the extension "IFTX" table.
   */
  IFT CreateExtensionTable() const;

  /*
   * Adds an encoded 'IFT ' table built from this IFT table to the font pointed
   * to by face. By default this will maintain the physical orderng of tables
   * already present in the font. If extension entries are present then an
   * extension table (IFTX) will also be added.
   */
  absl::StatusOr<common::FontData> AddToFont(
      hb_face_t* face, bool iftb_conversion = false) const;

 private:
  std::string url_template_;
  std::string extension_url_template_;
  uint32_t id_[4];
  PatchMap patch_map_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_IFT_TABLE_H_
