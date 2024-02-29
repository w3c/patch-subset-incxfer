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

namespace ift::proto {

constexpr hb_tag_t IFT_TAG = HB_TAG('I', 'F', 'T', ' ');
constexpr hb_tag_t IFTX_TAG = HB_TAG('I', 'F', 'T', 'X');

StatusOr<IFTTable> IFTTable::FromFont(hb_face_t* face) {
  FontData ift_table = FontHelper::TableData(face, IFT_TAG);
  if (ift_table.empty()) {
    return absl::NotFoundError("'IFT ' table not found in face.");
  }

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(ift_table.str(), out, false);
  if (!s.ok()) {
    return s;
  }

  // Check for an handle extension table if present.
  ift_table = FontHelper::TableData(face, IFTX_TAG);
  if (ift_table.empty()) {
    // No extension table
    return out;
  }

  s = Format2PatchMap::Deserialize(ift_table.str(), out, true);
  if (!s.ok()) {
    return s;
  }

  return out;
}

StatusOr<IFTTable> IFTTable::FromFont(const FontData& font) {
  hb_face_t* face = font.reference_face();
  auto s = IFTTable::FromFont(face);
  hb_face_destroy(face);
  return s;
}

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

StatusOr<FontData> IFTTable::AddToFont(hb_face_t* face,
                                       bool iftb_conversion) const {
  auto main = CreateMainTable();
  if (!main.ok()) {
    return main.status();
  }

  StatusOr<std::string> ext;
  std::optional<absl::string_view> ext_view;
  if (HasExtensionEntries()) {
    ext = CreateExtensionTable();
    if (!ext.ok()) {
      return ext.status();
    }
    ext_view = *ext;
  }

  return AddToFont(face, *main, ext_view, iftb_conversion);
}

StatusOr<std::string> IFTTable::CreateMainTable() const {
  return Format2PatchMap::Serialize(*this, false);
}

StatusOr<std::string> IFTTable::CreateExtensionTable() const {
  return Format2PatchMap::Serialize(*this, true);
}

void IFTTable::GetId(uint32_t out[4]) const {
  for (int i = 0; i < 4; i++) {
    out[i] = id_[i];
  }
}

bool IFTTable::HasExtensionEntries() const {
  for (const auto& e : GetPatchMap().GetEntries()) {
    if (e.extension_entry) {
      return true;
    }
  }
  return false;
}

void PrintTo(const IFTTable& table, std::ostream* os) {
  *os << "{" << std::endl;
  *os << "  url_template = " << table.GetUrlTemplate() << std::endl;
  *os << "  ext_url_template = " << table.GetExtensionUrlTemplate()
      << std::endl;
  *os << "  id = [" << table.id_[0] << " " << table.id_[1] << " "
      << table.id_[2] << " " << table.id_[3] << "]" << std::endl;
  *os << "  patch_map = ";
  PrintTo(table.GetPatchMap(), os);
  *os << std::endl;
  *os << "}";
}

}  // namespace ift::proto
