#include "ift/encoder/iftb_patch_creator.h"

#include "chunk.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "merger.h"

namespace ift::encoder {

using absl::Span;
using absl::StatusOr;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;

using iftb::merger;

StatusOr<std::vector<merger::glyphrec>> GetGlyfRecords(hb_face_t* face) {
  uint32_t num_glyphs = hb_face_get_glyph_count(face);
  std::vector<merger::glyphrec> out;
  out.resize(num_glyphs);

  for (uint32_t gid = 0; gid < num_glyphs; gid++) {
    auto glyph_data = FontHelper::GlyfData(face, gid);
    if (!glyph_data.ok()) {
      return glyph_data.status();
    }
    merger::glyphrec rec;
    rec.length = glyph_data->length();
    rec.offset = glyph_data->data();
    out[gid] = rec;
  }

  return out;
}

StatusOr<std::vector<merger::glyphrec>> GetGvarRecords(hb_face_t* face) {
  uint32_t num_glyphs = hb_face_get_glyph_count(face);
  std::vector<merger::glyphrec> out;
  out.resize(num_glyphs);

  for (uint32_t gid = 0; gid < num_glyphs; gid++) {
    auto glyph_data = FontHelper::GvarData(face, gid);
    if (!glyph_data.ok()) {
      return glyph_data.status();
    }
    merger::glyphrec rec;
    rec.length = glyph_data->length();
    rec.offset = glyph_data->data();
    out[gid] = rec;
  }

  return out;
}

StatusOr<FontData> IftbPatchCreator::CreatePatch(
    const FontData& font, uint32_t chunk_idx, Span<const uint32_t> id,
    const absl::flat_hash_set<uint32_t>& gids) {
  auto face = font.face();
  auto tags = FontHelper::GetTags(face.get());
  if (tags.contains(FontHelper::kCFF) || tags.contains(FontHelper::kCFF2)) {
    return absl::UnimplementedError("CFF support is not yet implemented.");
  }

  bool has_gvar = tags.contains(FontHelper::kGvar);
  auto glyf_recs = GetGlyfRecords(face.get());
  if (!glyf_recs.ok()) {
    return glyf_recs.status();
  }

  std::vector<merger::glyphrec> gvar_recs;
  if (has_gvar) {
    auto r = GetGvarRecords(face.get());
    if (!r.ok()) {
      return r.status();
    }
    gvar_recs = *r;
  }

  hb_set_unique_ptr gids_hb = make_hb_set();
  for (uint32_t gid : gids) {
    hb_set_add(gids_hb.get(), gid);
  }

  std::vector<uint8_t> patch;
  std::stringstream out;
  iftb::chunk::compile(out, chunk_idx, const_cast<uint32_t*>(id.data()),
                       gids_hb.get(), FontHelper::kGlyf, *glyf_recs,
                       has_gvar ? FontHelper::kGvar : 0, gvar_recs, 0);

  std::string encoded = iftb::chunk::encode(out);

  FontData out_data(encoded);
  return out_data;
}

}  // namespace ift::encoder