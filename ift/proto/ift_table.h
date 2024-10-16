#ifndef IFT_PROTO_IFT_TABLE_H_
#define IFT_PROTO_IFT_TABLE_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "common/font_data.h"
#include "hb.h"
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

  friend void PrintTo(const IFTTable& table, std::ostream* os);

  // TODO(garretrieger): add a separate extension id as well (like w/ URL
  // templates).
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

  bool operator==(const IFTTable& other) const {
    return url_template_ == other.url_template_ &&
           extension_url_template_ == other.extension_url_template_ &&
           id_[0] == other.id_[0] && id_[1] == other.id_[1] &&
           id_[2] == other.id_[2] && id_[3] == other.id_[3] &&
           patch_map_ == other.patch_map_;
  }

  /*
   * Adds an encoded 'IFT ' table built from this IFT table to the font pointed
   * to by face. By default this will maintain the physical orderng of tables
   * already present in the font. If extension entries are present then an
   * extension table (IFTX) will also be added.
   */
  absl::StatusOr<common::FontData> AddToFont(
      hb_face_t* face, bool iftb_conversion = false) const;

 private:
  /*
   * Adds an encoded 'IFT ' table built from the supplied proto to font pointed
   * to by face. By default this will maintain the physical ordering of tables
   * already present in the font. If iftb_conversion is set any "IFTB" tables
   * if present will be be removed and tables in the final font will be ordered
   * according to IFTB ordering requirements.
   */
  static absl::StatusOr<common::FontData> AddToFont(
      hb_face_t* face, absl::string_view ift_table,
      std::optional<absl::string_view> iftx_table,
      bool iftb_conversion = false);

  /*
   * Converts this abstract representation to the proto representation.
   * This method generates the proto for the main "IFT " table.
   */
  absl::StatusOr<std::string> CreateMainTable() const;

  /*
   * Converts this abstract representation to the proto representation.
   * This method generates the proto for the extension "IFTX" table.
   */
  absl::StatusOr<std::string> CreateExtensionTable() const;

  std::string url_template_;
  std::string extension_url_template_;
  uint32_t id_[4] = {0, 0, 0, 0};
  PatchMap patch_map_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_IFT_TABLE_H_
