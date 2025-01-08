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
#include "merger.h"

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
using iftb::merger;
using iftb::sfnt;

namespace ift {

StatusOr<flat_hash_set<uint32_t>> GlyphKeyedDiff::GidsInIftbPatch(
    const FontData& patch) {
  // Format of the patch:
  // 0:  uint32        version
  // 4:  uint32        reserved
  // 8:  uint32        id[4]
  // 24: uint32        chunkIndex
  // 28: uint32        length
  // 32: uint32        glyphCount
  // 36: uint8         tableCount
  // 37: uint16        GIDs[glyphCount]
  //     uint32        tables[tableCount]
  //     Offset32      offsets[glyphCount * tableCount]
  static constexpr int glyphCountOffset = 32;
  static constexpr int gidsArrayOffset = 37;

  merger merger;
  std::string uncompressed;
  if (HB_TAG('I', 'F', 'T', 'C') !=
      iftb::decodeBuffer(patch.data(), patch.size(), uncompressed)) {
    return absl::InvalidArgumentError("Unsupported chunk type.");
  }

  absl::string_view data = uncompressed;
  auto glyph_count = FontHelper::ReadUInt32(data.substr(glyphCountOffset));
  if (!glyph_count.ok()) {
    return absl::InvalidArgumentError("Failed to read glyph count.");
  }

  flat_hash_set<uint32_t> result;
  for (uint32_t i = 0; i < *glyph_count; i++) {
    auto gid = FontHelper::ReadUInt16(data.substr(gidsArrayOffset + 2 * i));
    if (!gid.ok()) {
      return absl::InvalidArgumentError(
          StrCat("Failed to read gid at index ", i));
    }
    result.insert(*gid);
  }

  return result;
}

StatusOr<CompatId> GlyphKeyedDiff::IdInIftbPatch(const FontData& patch) {
  static constexpr int idOffset = 8;
  std::string uncompressed;
  if (HB_TAG('I', 'F', 'T', 'C') !=
      iftb::decodeBuffer(patch.data(), patch.size(), uncompressed)) {
    return absl::InvalidArgumentError("Unsupported chunk type.");
  }

  absl::string_view data = uncompressed;
  uint32_t id_values[4];
  for (int i = 0; i < 4; i++) {
    auto val = FontHelper::ReadUInt32(data.substr(idOffset + i * 4));
    if (!val.ok()) {
      return val.status();
    }
    id_values[i] = *val;
  }

  return CompatId(id_values);
}

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
      return absl::InvalidArgumentError("Unsupported table type for glyph keyed diff.");
    }
  }

  auto face = font_.face();
  auto face_tags = FontHelper::GetTags(face.get());

  btree_set<hb_tag_t> processed_tags;
  std::string offset_data;
  std::string per_glyph_data;

  // TODO(garretrieger): add CFF support
  // TODO(garretrieger): add CFF2 support

  if (tags_.contains(FontHelper::kGlyf) &&
      face_tags.contains(FontHelper::kGlyf) &&
      face_tags.contains(FontHelper::kLoca)) {
    processed_tags.insert(FontHelper::kGlyf);

    for (auto gid : gids) {
      auto data = FontHelper::GlyfData(
          face.get(), gid);  // TODO is the padding trimmed in GlyfData?
      if (!data.ok()) {
        return data.status();
      }

      FontHelper::WriteUInt32(per_glyph_data.size(), offset_data);
      per_glyph_data += *data;
    }
  }

  // TODO(garretrieger): add gvar support

  // Add the trailing offset
  FontHelper::WriteUInt32(per_glyph_data.size(), offset_data);

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
