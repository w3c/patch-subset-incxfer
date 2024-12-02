#include "ift/glyph_keyed_diff.h"

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"
#include "merger.h"

using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using ift::proto::IFT;
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

Status GlyphKeyedDiff::IdInIftbPatch(const FontData& patch, uint32_t id_out[4]) {
  static constexpr int idOffset = 8;
  std::string uncompressed;
  if (HB_TAG('I', 'F', 'T', 'C') !=
      iftb::decodeBuffer(patch.data(), patch.size(), uncompressed)) {
    return absl::InvalidArgumentError("Unsupported chunk type.");
  }

  absl::string_view data = uncompressed;
  for (int i = 0; i < 4; i++) {
    auto val = FontHelper::ReadUInt32(data.substr(idOffset + i * 4));
    if (!val.ok()) {
      return val.status();
    }
    id_out[i] = *val;
  }

  return absl::OkStatus();
}

}  // namespace ift
