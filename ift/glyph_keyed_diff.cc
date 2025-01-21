#include "ift/glyph_keyed_diff.h"

#include <algorithm>
#include <cstdint>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/brotli_binary_diff.h"
#include "common/compat_id.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::BrotliBinaryDiff;
using common::CompatId;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using ift::proto::IFTTable;
using ift::proto::PatchMap;

namespace ift {

StatusOr<FontData> GlyphKeyedDiff::CreatePatch(
    const btree_set<uint32_t>& gids) const {
  // TODO(garretrieger): use write macros that check for overflows.
  std::string patch;
  FontHelper::WriteUInt32(HB_TAG('i', 'f', 'g', 'k'), patch);  // Format Tag
  FontHelper::WriteUInt32(0, patch);                           // Reserved.

  if (gids.empty()) {
    return absl::InvalidArgumentError(
        "There must be at least one gid in the requested patch.");
  }

  uint32_t max_gid = *std::max_element(gids.begin(), gids.end());
  if (max_gid > (1 << 24) - 1) {
    return absl::InvalidArgumentError("Larger then 24 bit gid requested.");
  }

  bool u16_gids = true;
  if (max_gid > (1 << 16) - 1) {
    u16_gids = false;
  }

  // Flags
  FontHelper::WriteUInt8(u16_gids ? 0b00000000 : 0b00000001, patch);
  base_compat_id_.WriteTo(patch);  // Compat ID

  auto uncompressed_data_stream = CreateDataStream(gids, u16_gids);
  if (!uncompressed_data_stream.ok()) {
    return uncompressed_data_stream.status();
  }

  FontData empty;
  FontData compressed_data_stream;
  auto status = brotli_diff_.Diff(empty, *uncompressed_data_stream,
                                  &compressed_data_stream);
  if (!status.ok()) {
    return status;
  }

  // Max Uncompressed Length
  FontHelper::WriteUInt32(uncompressed_data_stream->size(), patch);

  // Compressed Data Stream
  patch += compressed_data_stream.str();

  FontData result(patch);
  return result;
}

StatusOr<FontData> GlyphKeyedDiff::CreateDataStream(
    const btree_set<uint32_t>& gids, bool u16_gids) const {
  // check for unsupported tags.
  for (auto tag : tags_) {
    if (tag != FontHelper::kGlyf && tag != FontHelper::kGvar) {
      return absl::InvalidArgumentError(
          "Unsupported table type for glyph keyed diff.");
    }
  }

  auto face = font_.face();
  auto face_tags = FontHelper::GetTags(face.get());

  btree_set<hb_tag_t> processed_tags;
  std::string offset_data;
  std::string per_glyph_data;

  bool include_glyf = tags_.contains(FontHelper::kGlyf) &&
                      face_tags.contains(FontHelper::kGlyf) &&
                      face_tags.contains(FontHelper::kLoca);
  bool include_gvar = tags_.contains(FontHelper::kGvar) &&
                      face_tags.contains(FontHelper::kGvar);

  uint32_t glyph_count = gids.size();
  uint32_t glyph_id_width = u16_gids ? 2 : 3;
  uint32_t table_count = (include_glyf ? 1 : 0) + (include_gvar ? 1 : 0);
  uint32_t header_size = 5 + glyph_id_width * glyph_count + table_count * 4 +
                         4 * glyph_count * table_count + 4;

  if (tags_.contains(FontHelper::kCFF) &&
      face_tags.contains(FontHelper::kCFF)) {
    // TODO(garretrieger): add CFF support
    return absl::UnimplementedError(
        "CFF glyph keyed patching not yet implemented.");
  }

  if (tags_.contains(FontHelper::kCFF2) &&
      face_tags.contains(FontHelper::kCFF2)) {
    // TODO(garretrieger): add CFF2 support
    return absl::UnimplementedError(
        "CFF2 glyph keyed patching not yet implemented.");
  }

  if (include_glyf) {
    processed_tags.insert(FontHelper::kGlyf);

    for (auto gid : gids) {
      auto data = FontHelper::GlyfData(face.get(), gid);
      if (!data.ok()) {
        return data.status();
      }

      FontHelper::WriteUInt32(header_size + per_glyph_data.size(), offset_data);
      per_glyph_data += *data;
    }
  }

  if (include_gvar) {
    processed_tags.insert(FontHelper::kGvar);

    for (auto gid : gids) {
      auto data = FontHelper::GvarData(face.get(), gid);
      if (!data.ok()) {
        return data.status();
      }

      FontHelper::WriteUInt32(header_size + per_glyph_data.size(), offset_data);
      per_glyph_data += *data;
    }
  }

  // Add the trailing offset
  FontHelper::WriteUInt32(header_size + per_glyph_data.size(), offset_data);

  // Stream Construction
  std::string stream;
  FontHelper::WriteUInt32(gids.size(), stream);           // glyphCount
  FontHelper::WriteUInt8(processed_tags.size(), stream);  // tableCount

  // glyphIds
  for (auto gid : gids) {
    if (u16_gids) {
      FontHelper::WriteUInt16(gid, stream);
    } else {
      FontHelper::WriteUInt24(gid, stream);
    }
  }

  // tables
  for (auto tag : processed_tags) {
    FontHelper::WriteUInt32(tag, stream);
  }

  stream += offset_data;
  stream += per_glyph_data;

  FontData result(stream);
  return result;
}

}  // namespace ift
