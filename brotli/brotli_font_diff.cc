#include "brotli/brotli_font_diff.h"

#include "absl/types/span.h"
#include "brotli/brotli_stream.h"

using ::absl::Span;
using ::patch_subset::FontData;
using ::patch_subset::StatusCode;

namespace brotli {

Span<const uint8_t> to_span(hb_blob_t* blob) {
  unsigned length;
  const uint8_t* data =
      reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
  return Span<const uint8_t>(data, length);
}

class TableRange {

 public:
  TableRange(hb_face_t* base_face, hb_face_t* derived_face, hb_tag_t tag) {
    // TODO(grieger): use spans, eg. to_span above.
    hb_blob_t* table =
        hb_face_reference_table(derived_face, tag);
    derived_ = (const uint8_t*) hb_blob_get_data(table, &derived_length_);
    // TODO(garretrieger): blob should be retained until we're done with
    // derived_glyf
    hb_blob_destroy(table);

    table = hb_face_reference_table(base_face, tag);
    hb_blob_t* base = hb_face_reference_blob(base_face);
    const uint8_t* table_data = (const uint8_t*)hb_blob_get_data(table, nullptr);
    const uint8_t* base_data = (const uint8_t*)hb_blob_get_data(base, nullptr);
    // TODO(garretrieger): blob should be retained until we're done with
    // base_glyf
    base_table_offset_ = table_data - base_data;

    hb_blob_destroy(table);
    hb_blob_destroy(base);
  }

 private:
  const uint8_t* derived_;
  unsigned base_table_offset_;
  unsigned base_offset_ = 0;
  unsigned derived_offset_ = 0;
  unsigned derived_length_ = 0;
  unsigned length_ = 0;

 public:
  const uint8_t* data() { return derived_; }
  unsigned length() {
    return derived_length_;
  }

  void Extend(unsigned length) {
    length_ += length;
  }

  void CommitNew(BrotliStream& out) {
    out.insert_compressed(
        Span<const uint8_t>(derived_ + derived_offset_, length_));
    derived_offset_ += length_;
    length_ = 0;
  }

  void CommitExisting(BrotliStream& out) {
    out.insert_from_dictionary(base_table_offset_ + base_offset_, length_);
    base_offset_ += length_;
    derived_offset_ += length_;
    length_ = 0;
  }
};

/*
 * Writes out a brotli encoded copy of the 'derived' subsets glyf table using
 * the 'base' subset as a shared dictionary.
 *
 * Performs the comparison using the glyph ids in the plans for each subset and
 * does not actually compare any glyph bytes. Common ranges are glyphs are
 * encoded using backwards references to the base dictionary. Novel glyph data
 * found in 'derived' is encoded as compressed data without the use of the
 * shared dictionary.
 */
class GlyfDiff {
  // TODO(garretrieger): move GlyfDiff into it's own file.

 public:
  GlyfDiff(hb_subset_plan_t* base_plan, hb_face_t* base_face,
           hb_subset_plan_t* derived_plan, hb_face_t* derived_face)
      : glyf_range(base_face, derived_face, HB_TAG('g', 'l', 'y', 'f')),
        loca_range(base_face, derived_face, HB_TAG('l', 'o', 'c', 'a')),
        base_new_to_old(hb_subset_plan_new_to_old_glyph_mapping(base_plan)),
        derived_old_to_new(
            hb_subset_plan_old_to_new_glyph_mapping(derived_plan)) {

    hb_blob_t* head =
        hb_face_reference_table(derived_face, HB_TAG('h', 'e', 'a', 'd'));
    const char* head_data = hb_blob_get_data(head, nullptr);
    use_short_loca = !head_data[51];
    loca_width = use_short_loca ? 2 : 4;
    hb_blob_destroy(head);

    base_glyph_count = hb_face_get_glyph_count(base_face);
    derived_glyph_count = hb_face_get_glyph_count(derived_face);
    retain_gids = base_glyph_count < hb_map_get_population(base_new_to_old);
  }

 private:
  enum Mode {
    INIT = 0,
    NEW_DATA,
    EXISTING_DATA,
  } mode = INIT;

  bool loca_diverged = false;

  TableRange glyf_range;
  TableRange loca_range;

  unsigned base_gid = 0;
  unsigned derived_gid = 0;

  const hb_map_t* base_new_to_old;
  const hb_map_t* derived_old_to_new;

  unsigned base_glyph_count;
  unsigned derived_glyph_count;
  bool use_short_loca;
  unsigned loca_width;
  bool retain_gids;

 public:
  void MakeDiff(BrotliStream& out) {
    // Build loca and glyf together.
    BrotliStream glyf(out.window_bits(),
                      out.dictionary_size(),
                      out.uncompressed_size() + loca_range.length());

    // Notation:
    // base_gid:      glyph id in the base subset glyph space.
    // *_derived_gid: glyph id in the derived subset glyph space.
    // *_old_gid:     glyph id in the original font glyph space.

    while (derived_gid < derived_glyph_count) {
      unsigned base_derived_gid = BaseToDerivedGid(base_gid);
      switch (mode) {
        case INIT:
          StartRange(base_derived_gid);
          continue;

        case NEW_DATA:
          loca_diverged = true;
          if (base_derived_gid != derived_gid) {
            // Continue current range.
            glyf_range.Extend(GlyphLength(derived_gid));
            loca_range.Extend(loca_width);
            derived_gid++;
            continue;
          }

          CommitRange(out, glyf);
          StartRange(base_derived_gid);
          continue;

        case EXISTING_DATA:
          if (base_derived_gid == derived_gid) {
            // Continue current range.
            glyf_range.Extend(GlyphLength(derived_gid));
            loca_range.Extend(loca_width);
            derived_gid++;
            base_gid++;
            continue;
          }

          CommitRange(out, glyf);
          StartRange(base_derived_gid);
          continue;
      }
    }

    CommitRange(out, glyf);
    loca_range.Extend(loca_width); // Loca has num glyphs + 1 entries.
    if (loca_diverged) {
      loca_range.CommitNew(out);
    } else {
      loca_range.CommitExisting(out);
    }
    out.four_byte_align_uncompressed();

    out.append(glyf);
  }

 private:
  unsigned BaseToDerivedGid(unsigned gid) {
    if (retain_gids) {
      // If retain gids is set gids are equivalent in all three spaces.
      return gid < base_glyph_count ? gid : HB_MAP_VALUE_INVALID;
    }

    unsigned base_old_gid = hb_map_get(base_new_to_old, base_gid);
    return hb_map_get(derived_old_to_new, base_old_gid);
  }

  void CommitRange(BrotliStream& loca, BrotliStream& glyf) {
    switch (mode) {
      case NEW_DATA:
        glyf_range.CommitNew(glyf);
        break;
      case EXISTING_DATA:
        glyf_range.CommitExisting(glyf);
        if (!loca_diverged) {
          loca_range.CommitExisting(loca);
        }
        break;
      case INIT:
        return;
    }
  }

  void StartRange(unsigned base_derived_gid) {
    glyf_range.Extend(GlyphLength(derived_gid));
    loca_range.Extend(loca_width);

    if (base_derived_gid != derived_gid) {
      mode = NEW_DATA;
      loca_diverged = true;
    } else {
      mode = EXISTING_DATA;
      base_gid++;
    }

    derived_gid++;
  }

  // Length of glyph (in bytes) found in the derived subset.
  unsigned GlyphLength(unsigned gid) {
    const uint8_t* derived_loca = loca_range.data();

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

    unsigned off_1 = ((derived_loca[index] & 0xFF) << 24) |
                     ((derived_loca[index + 1] & 0xFF) << 16) |
                     ((derived_loca[index + 2] & 0xFF) << 8) |
                     (derived_loca[index + 3] & 0xFF);

    index += 4;
    unsigned off_2 = ((derived_loca[index] & 0xFF) << 24) |
                     ((derived_loca[index + 1] & 0xFF) << 16) |
                     ((derived_loca[index + 2] & 0xFF) << 8) |
                     (derived_loca[index + 3] & 0xFF);

    return off_2 - off_1;
  }
};

StatusCode BrotliFontDiff::Diff(
    hb_subset_plan_t* base_plan, hb_blob_t* base,
    hb_subset_plan_t* derived_plan, hb_blob_t* derived,
    FontData* patch) const  // TODO(garretrieger): write into sink.
{
  Span<const uint8_t> base_span = to_span(base);
  Span<const uint8_t> derived_span = to_span(derived);

  // get a 'real' (non facebuilder) face for the faces.
  hb_face_t* derived_face = hb_face_create(derived, 0);
  hb_face_t* base_face = hb_face_create(base, 0);

  // TODO(garretrieger): Compute a window size based on the non-glyf + base data
  // sizes.
  BrotliStream out(22, base_span.size());

  hb_blob_t* base_glyf =
      hb_face_reference_table(base_face, HB_TAG('g', 'l', 'y', 'f'));
  hb_blob_t* base_loca =
      hb_face_reference_table(base_face, HB_TAG('l', 'o', 'c', 'a'));
  hb_blob_t* derived_glyf =
      hb_face_reference_table(derived_face, HB_TAG('g', 'l', 'y', 'f'));
  hb_blob_t* derived_loca =
      hb_face_reference_table(derived_face, HB_TAG('l', 'o', 'c', 'a'));

  Span<const uint8_t> base_glyf_span = to_span(base_glyf);
  Span<const uint8_t> base_loca_span = to_span(base_loca);
  Span<const uint8_t> derived_glyf_span = to_span(derived_glyf);
  Span<const uint8_t> derived_loca_span = to_span(derived_loca);

  unsigned base_loca_offset = base_loca_span.data() - base_span.data();
  unsigned derived_loca_offset = derived_loca_span.data() - derived_span.data();
  unsigned derived_glyf_offset = derived_glyf_span.data() - derived_span.data();

  if (derived_loca_span.data() + derived_loca_span.size() != derived_glyf_span.data()) {
    LOG(WARNING) << "derived loca must immeditately preceed glyf.";
    return StatusCode::kInternal;
  }

  if (base_loca_span.data() + base_loca_span.size() != base_glyf_span.data()) {
    LOG(WARNING) << "base loca must immeditately preceed glyf.";
    return StatusCode::kInternal;
  }

  out.insert_compressed_with_partial_dict(
      derived_span.subspan(0, derived_loca_offset),
      base_span.subspan(0, base_loca_offset));

  GlyfDiff glyf_diff(base_plan, base_face, derived_plan, derived_face);
  glyf_diff.MakeDiff(out);

  if (derived_span.size() > derived_glyf_offset + derived_glyf_span.size()) {
    out.insert_compressed(derived_span.subspan(
        derived_glyf_offset + derived_glyf_span.size(),
        derived_span.size() - derived_glyf_offset - derived_glyf_span.size()));
  }

  out.end_stream();

  patch->copy((const char*)out.compressed_data().data(),
              out.compressed_data().size());

  hb_blob_destroy(base_glyf);
  hb_blob_destroy(base_loca);
  hb_blob_destroy(derived_glyf);
  hb_blob_destroy(derived_loca);

  hb_face_destroy(base_face);
  hb_face_destroy(derived_face);

  return StatusCode::kOk;
}

}  // namespace brotli
