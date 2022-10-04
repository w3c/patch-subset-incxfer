#include "util/brotli_font_diff.h"

#include "absl/types/span.h"
#include "util/brotli_stream.h"

using ::absl::Span;
using ::patch_subset::StatusCode;
using ::patch_subset::FontData;

namespace util {

// TODO(garretrieger): move GlyfDiff into it's own file.
class GlyfDiff {

 public:
  GlyfDiff(hb_subset_plan_t* base_plan,
           hb_face_t* base_face_,
           hb_subset_plan_t* derived_plan,
           hb_face_t* derived_face_) :
      base_face(base_face_), derived_face(derived_face_),
      base_new_to_old(hb_subset_plan_new_to_old_glyph_mapping(base_plan)),
      derived_old_to_new(hb_subset_plan_old_to_new_glyph_mapping(derived_plan))
  {
    hb_blob_t* loca = hb_face_reference_table(derived_face, HB_TAG('l', 'o', 'c', 'a'));
    derived_loca = (const uint8_t*) hb_blob_get_data(loca, nullptr);
    // TODO(garretrieger): blob should be retained until we're done with derived_loca
    hb_blob_destroy(loca);

    hb_blob_t* glyf = hb_face_reference_table(derived_face, HB_TAG('g', 'l', 'y', 'f'));
    derived_glyf = (const uint8_t*) hb_blob_get_data(glyf, nullptr);
    // TODO(garretrieger): blob should be retained until we're done with derived_glyf
    hb_blob_destroy(glyf);

    glyf = hb_face_reference_table(base_face, HB_TAG('g', 'l', 'y', 'f'));
    hb_blob_t* base = hb_face_reference_blob(base_face);
    const uint8_t* glyf_data = (const uint8_t*) hb_blob_get_data(glyf, nullptr);
    const uint8_t* base_data = (const uint8_t*) hb_blob_get_data(base, nullptr);
    base_glyf_offset = glyf_data - base_data;

    hb_blob_destroy(glyf);
    hb_blob_destroy(base);

    hb_blob_t* head = hb_face_reference_table(derived_face, HB_TAG('h', 'e', 'a', 'd'));
    const char* head_data = hb_blob_get_data(head, nullptr);
    use_short_loca = !head_data[51];
    hb_blob_destroy(head);
  }

 private:
  enum Mode {
    INIT = 0,
    NEW_DATA,
    EXISTING_DATA,
  } mode = INIT;
  unsigned base_offset = 0;
  unsigned derived_offset = 0;
  unsigned length = 0;

  unsigned base_gid = 0;
  unsigned base_derived_gid = 0;
  unsigned derived_gid = 0;

  hb_face_t* base_face;
  unsigned base_glyf_offset;

  hb_face_t* derived_face;

  const hb_map_t* base_new_to_old;
  const hb_map_t* derived_old_to_new;

  const uint8_t* derived_glyf;
  const uint8_t* derived_loca;
  bool use_short_loca;

 public:
  void MakeDiff(BrotliStream& out) {

    // Notation:
    // base_gid:      glyph id in the base subset glyph space.
    // *_derived_gid: glyph id in the derived subset glyph space.
    // *_old_gid:     glyph id in the original font glyph space.

    // TODO(garretrieger): this has issues when retain gids is set.

    unsigned base_glyph_count = hb_face_get_glyph_count(base_face);
    unsigned derived_glyph_count = hb_face_get_glyph_count(derived_face);
    // TODO(garretrieger): this termination condition only works if the font is formed
    //                     to expectations (that you consume all base and derived glyphs)
    while (base_gid < base_glyph_count ||
           derived_gid < derived_glyph_count) {
      unsigned base_old_gid = hb_map_get(base_new_to_old, base_gid);
      if (base_old_gid == HB_MAP_VALUE_INVALID && base_gid < base_glyph_count) {
        // This is a retain gids font, so the gid's don't change between subsets.
        base_derived_gid = base_gid;
      } else {
        base_derived_gid = hb_map_get(derived_old_to_new, base_old_gid);
      }

      switch (mode) {
        case INIT:
          StartRange();
          continue;

        case NEW_DATA:
          if (base_derived_gid != derived_gid) {
            // Continue current range.
            length += GlyphLength(derived_gid);
            derived_gid++;
            continue;
          }

          CommitRange(out);
          StartRange();
          continue;

        case EXISTING_DATA:
          if (base_derived_gid == derived_gid) {
            // Continue current range.
            length += GlyphLength(derived_gid);
            derived_gid++;
            base_gid++;
            continue;
          }

          CommitRange(out);
          StartRange();
          continue;
      }
    }

    CommitRange(out);
  }

  void CommitRange(BrotliStream& out) {
    switch (mode) {
      case NEW_DATA:
        // TODO(garretrieger): compress this data, don't use a dictionary.
        out.insert_uncompressed(Span<const uint8_t>(derived_glyf + derived_offset,
                                                    length));
        break;
      case EXISTING_DATA:
        out.insert_from_dictionary(base_glyf_offset + base_offset, length);
        base_offset += length;
        break;
      case INIT:
        return;
    }
    derived_offset += length;
  }

  void StartRange() {
    length = GlyphLength(derived_gid);

    if (base_derived_gid != derived_gid) {
      mode = NEW_DATA;
    } else {
      mode = EXISTING_DATA;
      base_gid++;
    }

    derived_gid++;
  }

  // Length of glyph (in bytes) found in the derived subset.
  unsigned GlyphLength(unsigned gid) {
    if (use_short_loca) {
      unsigned index = gid * 2;

      unsigned off_1 = ((derived_loca[index] & 0xFF) << 8) |
                       (derived_loca[index + 1] & 0xFF);

      index += 2;
      unsigned off_2 = ((derived_loca[index] & 0xFF) << 8) |
                       (derived_loca[index + 1] & 0xFF);

      return (off_2 * 2) - (off_1 * 2);
    }

    unsigned index = gid * 4;

    unsigned off_1 =
        ((derived_loca[index] & 0xFF) << 24) |
        ((derived_loca[index+1] & 0xFF) << 16) |
        ((derived_loca[index+2] & 0xFF) << 8) |
        (derived_loca[index+3] & 0xFF);

    index += 4;
    unsigned off_2 =
        ((derived_loca[index] & 0xFF) << 24) |
        ((derived_loca[index+1] & 0xFF) << 16) |
        ((derived_loca[index+2] & 0xFF) << 8) |
        (derived_loca[index+3] & 0xFF);

    return off_2 - off_1;
  }
};

StatusCode BrotliFontDiff::Diff(hb_subset_plan_t* base_plan,
                                hb_face_t* base_face,
                                hb_subset_plan_t* derived_plan,
                                hb_face_t* derived_face,
                                FontData* patch) const
{
  hb_blob_t* base = hb_face_reference_blob(base_face);
  hb_blob_t* derived = hb_face_reference_blob(derived_face);

  // get a 'real' (non facebuilder) face for the faces.
  derived_face = hb_face_create(derived, 0);
  base_face = hb_face_create(base, 0);

  BrotliStream out(22, hb_blob_get_length(base));

  hb_blob_t* glyf = hb_face_reference_table(derived_face, HB_TAG('g', 'l', 'y', 'f'));
  unsigned glyf_length;
  unsigned derived_length;
  const uint8_t* glyf_data = (const uint8_t*) hb_blob_get_data(glyf, &glyf_length);
  const uint8_t* derived_data = (const uint8_t*) hb_blob_get_data(derived, &derived_length);
  unsigned glyf_offset = glyf_data - derived_data;

  // TODO(garretrieger): insert the non-glyf data by compressing using the regular encoder
  //                     against a partial dictionary.
  out.insert_uncompressed(Span<const uint8_t>(derived_data, glyf_offset));

  GlyfDiff glyf_diff(base_plan, base_face, derived_plan, derived_face);
  glyf_diff.MakeDiff(out);

  if (derived_length > glyf_offset + glyf_length) {
    out.insert_uncompressed(Span<const uint8_t>(glyf_data + glyf_length,
                                                derived_length - glyf_offset - glyf_length));
  }

  out.end_stream();

  patch->copy((const char*) out.compressed_data().data(),
              out.compressed_data().size());

  hb_blob_destroy(derived);
  hb_face_destroy(derived_face);
  hb_face_destroy(base_face);
  hb_blob_destroy(base);

  return StatusCode::kOk;
}

}  // namespace util
