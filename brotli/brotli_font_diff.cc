#include "brotli/brotli_font_diff.h"

#include "absl/types/span.h"
#include "brotli/brotli_stream.h"
#include "brotli/glyf_differ.h"
#include "brotli/loca_differ.h"
#include "brotli/table_range.h"

using ::absl::Span;
using ::patch_subset::FontData;
using ::patch_subset::StatusCode;

namespace brotli {

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
class DiffDriver {
  // TODO(garretrieger): move this too it's own file.

  struct RangeAndDiffer {
    RangeAndDiffer(hb_face_t* base_face, hb_face_t* derived_face, hb_tag_t tag,
                   const BrotliStream& base_stream, TableDiffer* differ_)
        : range(base_face, derived_face, tag, base_stream), differ(differ_) {}

    TableRange range;
    std::unique_ptr<TableDiffer> differ;
  };

 public:
  DiffDriver(hb_subset_plan_t* base_plan, hb_face_t* base_face,
             hb_subset_plan_t* derived_plan, hb_face_t* derived_face,
             BrotliStream& stream)
      : out(stream),
        base_new_to_old(hb_subset_plan_new_to_old_glyph_mapping(base_plan)),
        derived_old_to_new(
            hb_subset_plan_old_to_new_glyph_mapping(derived_plan)) {
    hb_blob_t* head =
        hb_face_reference_table(derived_face, HB_TAG('h', 'e', 'a', 'd'));
    const char* head_data = hb_blob_get_data(head, nullptr);
    unsigned use_short_loca = !head_data[51];
    hb_blob_destroy(head);

    base_glyph_count = hb_face_get_glyph_count(base_face);
    derived_glyph_count = hb_face_get_glyph_count(derived_face);
    retain_gids = base_glyph_count < hb_map_get_population(base_new_to_old);

    // TODO(garretrieger): add these from a set of tags and in the order of the
    // tables
    //                     in the actual font.
    differs.push_back(RangeAndDiffer(base_face, derived_face,
                                     HB_TAG('l', 'o', 'c', 'a'), stream,
                                     new LocaDiffer(use_short_loca)));

    differs.push_back(RangeAndDiffer(
        base_face, derived_face, HB_TAG('g', 'l', 'y', 'f'), stream,
        new GlyfDiffer(
            TableRange::to_span(derived_face, HB_TAG('l', 'o', 'c', 'a')),
            use_short_loca)));
  }

 private:
  std::vector<RangeAndDiffer> differs;

  BrotliStream& out;

  unsigned base_gid = 0;
  unsigned derived_gid = 0;

  const hb_map_t* base_new_to_old;
  const hb_map_t* derived_old_to_new;

  unsigned base_glyph_count;
  unsigned derived_glyph_count;

  bool retain_gids;

 public:
  void MakeDiff() {
    // Notation:
    // base_gid:      glyph id in the base subset glyph space.
    // *_derived_gid: glyph id in the derived subset glyph space.
    // *_old_gid:     glyph id in the original font glyph space.

    while (derived_gid < derived_glyph_count) {
      unsigned base_derived_gid = BaseToDerivedGid(base_gid);

      for (auto& range_and_differ : differs) {
        TableDiffer* differ = range_and_differ.differ.get();
        TableRange& range = range_and_differ.range;

        bool was_new_data = differ->IsNewData();
        unsigned length =
            differ->Process(derived_gid, base_gid, base_derived_gid);

        if (derived_gid > 0 && was_new_data != differ->IsNewData()) {
          if (was_new_data) {
            range.CommitNew();
          } else {
            range.CommitExisting();
          }
        }

        range.Extend(length);
      }

      if (base_derived_gid == derived_gid) {
        base_gid++;
      }
      derived_gid++;
    }

    // Finalize and commit any outstanding changes.
    for (auto& range_and_differ : differs) {
      TableDiffer* differ = range_and_differ.differ.get();
      TableRange& range = range_and_differ.range;
      range.Extend(differ->Finalize());
      if (differ->IsNewData()) {
        range.CommitNew();
      } else {
        range.CommitExisting();
      }
      range.stream().four_byte_align_uncompressed();
      out.append(range.stream());
    }
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
};

StatusCode BrotliFontDiff::Diff(
    hb_subset_plan_t* base_plan, hb_blob_t* base,
    hb_subset_plan_t* derived_plan, hb_blob_t* derived,
    FontData* patch) const  // TODO(garretrieger): write into sink.
{
  Span<const uint8_t> base_span = TableRange::to_span(base);
  Span<const uint8_t> derived_span = TableRange::to_span(derived);

  // get a 'real' (non facebuilder) face for the faces.
  hb_face_t* derived_face = hb_face_create(derived, 0);
  hb_face_t* base_face = hb_face_create(base, 0);

  // TODO(garretrieger): Compute a window size based on the non-glyf + base data
  // sizes.
  BrotliStream out(22, base_span.size());

  Span<const uint8_t> base_glyf_span = TableRange::padded_table_span(
      TableRange::to_span(base_face, HB_TAG('g', 'l', 'y', 'f')));
  Span<const uint8_t> base_loca_span = TableRange::padded_table_span(
      TableRange::to_span(base_face, HB_TAG('l', 'o', 'c', 'a')));
  Span<const uint8_t> derived_glyf_span = TableRange::padded_table_span(
      TableRange::to_span(derived_face, HB_TAG('g', 'l', 'y', 'f')));
  Span<const uint8_t> derived_loca_span = TableRange::padded_table_span(
      TableRange::to_span(derived_face, HB_TAG('l', 'o', 'c', 'a')));

  unsigned base_loca_offset =
      TableRange::table_offset(base_face, HB_TAG('l', 'o', 'c', 'a'));
  unsigned derived_loca_offset =
      TableRange::table_offset(derived_face, HB_TAG('l', 'o', 'c', 'a'));
  unsigned derived_glyf_offset =
      TableRange::table_offset(derived_face, HB_TAG('g', 'l', 'y', 'f'));

  if (derived_loca_span.data() + derived_loca_span.size() !=
      derived_glyf_span.data()) {
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

  DiffDriver diff_driver(base_plan, base_face, derived_plan, derived_face, out);
  diff_driver.MakeDiff();

  if (derived_span.size() > derived_glyf_offset + derived_glyf_span.size()) {
    out.insert_compressed(derived_span.subspan(
        derived_glyf_offset + derived_glyf_span.size(),
        derived_span.size() - derived_glyf_offset - derived_glyf_span.size()));
  }

  out.end_stream();

  patch->copy((const char*)out.compressed_data().data(),
              out.compressed_data().size());

  hb_face_destroy(base_face);
  hb_face_destroy(derived_face);

  return StatusCode::kOk;
}

}  // namespace brotli
