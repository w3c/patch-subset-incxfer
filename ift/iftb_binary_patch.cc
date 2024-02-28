#include "ift/iftb_binary_patch.h"

#include <vector>

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

StatusOr<flat_hash_set<uint32_t>> IftbBinaryPatch::GidsInPatch(
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

Status IftbBinaryPatch::IdInPatch(const FontData& patch, uint32_t id_out[4]) {
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

Status IftbBinaryPatch::Patch(const FontData& font_base, const FontData& patch,
                              FontData* font_derived /* OUT */) const {
  std::vector<FontData> patches;
  patches.emplace_back();
  patches[0].shallow_copy(patch);
  return Patch(font_base, patches, font_derived);
}

StatusOr<uint32_t> get_chunk_index(const FontData& patch) {
  if (patch.size() < 28) {
    return absl::InvalidArgumentError(
        "Can't read chunk index in patch, too short.");
  }

  return FontHelper::ReadUInt32(patch.str().substr(24, 4));
}

Status IftbBinaryPatch::Patch(const FontData& font_base,
                              const std::vector<FontData>& patches,
                              FontData* font_derived /* OUT */) const {
  // TODO(garretrieger): this makes many unnessecary copies of data.
  //   Optimize to avoid them.
  merger merger;
  auto ift_table = IFTTable::FromFont(font_base);

  if (!ift_table.ok()) {
    return ift_table.status();
  }

  uint32_t id[4];
  ift_table->GetId(id);
  merger.setID(id);

  flat_hash_set<uint32_t> patch_indices;
  for (const FontData& patch : patches) {
    auto idx = get_chunk_index(patch);
    // TODO(garretrieger): validate read chunk index exists in ift_table.
    if (!idx.ok()) {
      return idx.status();
    }
    patch_indices.insert(*idx);
    if (HB_TAG('I', 'F', 'T', 'C') !=
        iftb::decodeBuffer(patch.data(), patch.size(),
                           merger.stringForChunk(*idx))) {
      return absl::InvalidArgumentError("Unsupported chunk type.");
    }
  }

  if (!merger.unpackChunks()) {
    return absl::InvalidArgumentError("Failed to unpack the chunks.");
  }

  sfnt sfnt;
  sfnt.setBuffer((char*)font_base.data(), font_base.size());
  if (!sfnt.read()) {
    return absl::InvalidArgumentError("Failed to read input font file.");
  }

  hb_face_unique_ptr face = font_base.face();
  unsigned num_glyphs = hb_face_get_glyph_count(face.get());

  uint32_t new_length = merger.calcLayout(
      sfnt, num_glyphs, 0 /* TODO(garretrieger): add CFF charstrings offset */);
  if (!new_length) {
    return absl::InvalidArgumentError(
        "Calculating layout before merge failed.");
  }

  // TODO(garretrieger): merge can use the old buffer as the new buffer,
  // assuming
  //  there is enough free space in it. May want to utilize this with a larger
  //  preallocation for the old buffer.
  std::string new_font_data;
  new_font_data.resize(new_length, 0);

  if (!merger.merge(sfnt, (char*)font_base.data(), new_font_data.data())) {
    return absl::InvalidArgumentError("IFTB Patch merging failed.");
  }

  // The above merge will have changed sfnt's buffer to new_font_data.
  // sfnt.write() needs to be called to realize table directory changes
  sfnt.write(false);

  bool touched_ext = false;
  for (uint32_t patch_index : patch_indices) {
    auto modification = ift_table->GetPatchMap().RemoveEntries(patch_index);
    touched_ext =
        touched_ext || (modification == PatchMap::MODIFIED_EXTENSION ||
                        modification == PatchMap::MODIFIED_BOTH);
  }

  hb_blob_unique_ptr blob =
      make_hb_blob(hb_blob_create(new_font_data.data(), new_length,
                                  HB_MEMORY_MODE_READONLY, nullptr, nullptr));
  face = make_hb_face(hb_face_create(blob.get(), 0));

  auto result = ift_table->AddToFont(face.get());

  if (!result.ok()) {
    return result.status();
  }

  font_derived->shallow_copy(*result);
  return absl::OkStatus();
}

}  // namespace ift
