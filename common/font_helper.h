#ifndef COMMON_FONT_HELPER_H_
#define COMMON_FONT_HELPER_H_

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "hb.h"

namespace common {

struct CompareTableOffsets {
  hb_face_t* face;
  CompareTableOffsets(hb_face_t* f) { face = f; }

  uint32_t table_offset(hb_tag_t tag) const {
    hb_blob_t* font = hb_face_reference_blob(face);
    hb_blob_t* table = hb_face_reference_table(face, tag);

    const uint8_t* font_ptr = (const uint8_t*)hb_blob_get_data(font, nullptr);
    const uint8_t* table_ptr = (const uint8_t*)hb_blob_get_data(table, nullptr);
    uint32_t offset = table_ptr - font_ptr;

    hb_blob_destroy(font);
    hb_blob_destroy(table);

    return offset;
  }

  bool operator()(hb_tag_t a, hb_tag_t b) const {
    return table_offset(a) < table_offset(b);
  }
};

class FontHelper {
 public:
  constexpr static hb_tag_t kIFT = HB_TAG('I', 'F', 'T', ' ');
  constexpr static hb_tag_t kLoca = HB_TAG('l', 'o', 'c', 'a');
  constexpr static hb_tag_t kGlyf = HB_TAG('g', 'l', 'y', 'f');
  constexpr static hb_tag_t kCFF = HB_TAG('C', 'F', 'F', ' ');

  static absl::StatusOr<uint32_t> ReadUInt32(absl::string_view value) {
    if (value.size() < 4) {
      return absl::InvalidArgumentError("Need at least 4 bytes");
    }
    return (((uint32_t)value[0]) << 24) + (((uint32_t)value[1]) << 16) +
           (((uint32_t)value[2]) << 8) + ((uint32_t)value[3]);
  }

  static absl::StatusOr<absl::string_view> Loca(hb_face_t* face) {
    hb_blob_t* loca_blob = hb_face_reference_table(face, FontHelper::kLoca);
    uint32_t loca_length = 0;
    const uint8_t* loca = reinterpret_cast<const uint8_t*>(
        hb_blob_get_data(loca_blob, &loca_length));
    if (!loca_length) {
      return absl::NotFoundError("loca table not found in face.");
    }

    absl::string_view result((const char*)loca, loca_length);

    hb_blob_destroy(loca_blob);
    return result;
  }

  static absl::flat_hash_set<uint32_t> GetTags(hb_face_t* face);
  static std::vector<hb_tag_t> GetOrderedTags(hb_face_t* face);
  static std::vector<std::string> ToStrings(const std::vector<hb_tag_t>& input);
};

}  // namespace common

#endif  // COMMON_FONT_HELPER_H_