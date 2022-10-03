#include "util/brotli_font_diff.h"

#include "absl/types/span.h"
#include "util/brotli_stream.h"

using ::absl::Span;
using ::patch_subset::StatusCode;
using ::patch_subset::FontData;

namespace util {

static void DiffGlyf(hb_subset_plan_t* base_plan,
                     hb_face_t* base_face,
                     hb_subset_plan_t* derived_plan,
                     hb_face_t* derived_face,
                     BrotliStream& out) {
  const hb_map_t* base_new_to_old = hb_subset_plan_new_to_old_glyph_mapping(base_plan);
  const hb_map_t* derived_old_to_new = hb_subset_plan_old_to_new_glyph_mapping(derived_plan);

  // Notation:
  // base_gid:      glyph id in the base subset glyph space.
  // *_derived_gid: glyph id in the derived subset glyph space.
  // *_old_gid:     glyph id in the original font glyph space.

  unsigned offset = (unsigned) -1;
  unsigned length = 0;
  unsigned base_gid = 0;
  unsigned derived_gid = 0;
  while (base_gid < hb_face_get_glyph_count(base_face) &&
         derived_gid < hb_face_get_glyph_count(derived_face)) {
    unsigned base_old_gid = hb_map_get(base_new_to_old, base_gid);
    unsigned base_derived_gid = hb_map_get(derived_old_to_new, base_old_gid);
  }
}

StatusCode BrotliFontDiff::Diff(hb_subset_plan_t* base_plan,
                                hb_face_t* base_face,
                                hb_subset_plan_t* derived_plan,
                                hb_face_t* derived_face,
                                FontData* patch) const
{
  hb_blob_t* base = hb_face_reference_blob(base_face);
  hb_blob_t* derived = hb_face_reference_blob(derived_face);
  derived_face = hb_face_create(derived, 0); // get a 'real' (non facebuilder) face for derived face.

  BrotliStream out(22);

  hb_blob_t* glyf = hb_face_reference_table(derived_face, HB_TAG('g', 'l', 'y', 'f'));
  unsigned glyf_length;
  unsigned derived_length;
  const uint8_t* glyf_data = (const uint8_t*) hb_blob_get_data(glyf, &glyf_length);
  const uint8_t* derived_data = (const uint8_t*) hb_blob_get_data(derived, &derived_length);
  unsigned glyf_offset = glyf_data - derived_data;

  // TODO(garretrieger): actually diff
  // Algo:
  // - output uncompressed segments for non 'glyf' parts of the file.
  // - find the ranges of glyphs in the base, and in derived
  // - walk the ranges in parallel output backward refs or uncompressed meta-blocks.

  out.insert_uncompressed(Span<const uint8_t>(derived_data, glyf_offset));
  out.insert_uncompressed(Span<const uint8_t>(glyf_data, glyf_length));
  if (derived_length > glyf_offset + glyf_length) {
    out.insert_uncompressed(Span<const uint8_t>(glyf_data + glyf_length,
                                                derived_length - glyf_offset - glyf_length));
  }

  out.end_stream();

  patch->copy((const char*) out.compressed_data().data(),
              out.compressed_data().size());

  hb_blob_destroy(derived);
  hb_face_destroy(derived_face);
  hb_blob_destroy(base);

  return StatusCode::kOk;
}

}  // namespace util
