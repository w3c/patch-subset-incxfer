#include "ift/proto/ift_table.h"

#include <google/protobuf/text_format.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/compat_id.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "hb.h"
#include "ift/proto/format_2_patch_map.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;
using common::CompatId;

namespace ift::proto {

constexpr hb_tag_t IFT_TAG = HB_TAG('I', 'F', 'T', ' ');
constexpr hb_tag_t IFTX_TAG = HB_TAG('I', 'F', 'T', 'X');

void move_tag_to_back(std::vector<hb_tag_t>& tags, hb_tag_t tag) {
  auto it = std::find(tags.begin(), tags.end(), tag);
  if (it != tags.end()) {
    tags.erase(it);
    tags.push_back(tag);
  }
}

StatusOr<FontData> IFTTable::AddToFont(
    hb_face_t* face, absl::string_view ift_table,
    std::optional<absl::string_view> iftx_table, bool iftb_conversion) {
  std::vector<hb_tag_t> tags = FontHelper::GetOrderedTags(face);
  hb_face_t* new_face = hb_face_builder_create();
  for (hb_tag_t tag : tags) {
    if (iftb_conversion && tag == FontHelper::kIFTB) {
      // drop IFTB if we're doing an IFTB conversion.
      continue;
    }
    hb_blob_t* blob = hb_face_reference_table(face, tag);
    hb_face_builder_add_table(new_face, tag, blob);
    hb_blob_destroy(blob);
  }

  if (iftb_conversion) {
    auto it = std::find(tags.begin(), tags.end(), FontHelper::kIFTB);
    if (it != tags.end()) {
      tags.erase(it);
    }
  }

  hb_blob_t* blob =
      hb_blob_create_or_fail(ift_table.data(), ift_table.size(),
                             HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  if (!blob) {
    return absl::InternalError(
        "Failed to allocate memory for serialized IFT table.");
  }
  hb_face_builder_add_table(new_face, IFT_TAG, blob);
  hb_blob_destroy(blob);

  if (std::find(tags.begin(), tags.end(), IFT_TAG) == tags.end()) {
    // Add 'IFT ' tag if it wasn't added above.
    tags.push_back(IFT_TAG);
  }

  std::string serialized_ext;
  if (iftx_table.has_value()) {
    blob = hb_blob_create_or_fail(iftx_table->data(), iftx_table->size(),
                                  HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    if (!blob) {
      return absl::InternalError(
          "Failed to allocate memory for serialized IFT table.");
    }
    hb_face_builder_add_table(new_face, IFTX_TAG, blob);
    hb_blob_destroy(blob);

    if (std::find(tags.begin(), tags.end(), IFTX_TAG) == tags.end()) {
      // Add 'IFTX' tag if it wasn't added above.
      tags.push_back(IFTX_TAG);
    }
  }

  if (iftb_conversion) {
    // requirements:
    // - gvar before glyf.
    // - glyf before loca.
    // - loca at end of file.
    // - CFF/CFF2 at end of file.
    move_tag_to_back(tags, HB_TAG('g', 'v', 'a', 'r'));
    move_tag_to_back(tags, HB_TAG('g', 'l', 'y', 'f'));
    move_tag_to_back(tags, HB_TAG('l', 'o', 'c', 'a'));
    move_tag_to_back(tags, HB_TAG('C', 'F', 'F', ' '));
    move_tag_to_back(tags, HB_TAG('C', 'F', 'F', '2'));
  }

  tags.push_back(0);  // null terminate the array as expected by hb.
  hb_face_builder_sort_tables(new_face, tags.data());

  blob = hb_face_reference_blob(new_face);
  hb_face_destroy(new_face);
  FontData new_font_data(blob);
  hb_blob_destroy(blob);

  return new_font_data;
}

absl::StatusOr<common::FontData> IFTTable::AddToFont(
      hb_face_t* face, const IFTTable& main, std::optional<const IFTTable*> extension, bool iftb_conversion) {
  auto main_bytes = Format2PatchMap::Serialize(main);
  if (!main_bytes.ok()) {
    return main_bytes.status();
  }

  StatusOr<std::string> ext_bytes;
  std::optional<absl::string_view> ext_view;
  if (extension) {
    auto ext_bytes = Format2PatchMap::Serialize(**extension);
    if (!ext_bytes.ok()) {
      return ext_bytes.status();
    }
    ext_view = *ext_bytes;
  }

  return AddToFont(face, *main_bytes, ext_view, iftb_conversion);
}

CompatId IFTTable::GetId() const {
  return id_;  
}

void PrintTo(const IFTTable& table, std::ostream* os) {
  *os << "{" << std::endl;
  *os << "  url_template = " << table.GetUrlTemplate() << std::endl;
  *os << "  id = ";
  PrintTo(table.id_, os);
  *os << std::endl;
  *os << "  patch_map = ";
  PrintTo(table.GetPatchMap(), os);
  *os << std::endl;
  *os << "}";
}

}  // namespace ift::proto
