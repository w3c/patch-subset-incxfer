#include "brotli/brotli_font_diff.h"

#include "absl/types/span.h"
#include "brotli/brotli_stream.h"
#include "brotli/glyf_differ.h"
#include "brotli/hmtx_differ.h"
#include "brotli/loca_differ.h"
#include "brotli/table_range.h"
#include "patch_subset/hb_set_unique_ptr.h"

using ::absl::Span;
using ::patch_subset::FontData;
using ::patch_subset::hb_set_unique_ptr;
using ::patch_subset::StatusCode;

namespace brotli {

static bool HasTable(hb_face_t* face, hb_tag_t tag) {
  hb_blob_t* table = hb_face_reference_table(face, tag);
  bool non_empty = (table != hb_blob_get_empty());
  hb_blob_destroy(table);
  return non_empty;
}

static bool HasTable(hb_face_t* base, hb_face_t* derived, hb_tag_t tag) {
  return HasTable(base, tag) && HasTable(derived, tag);
}

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
             const hb_set_t* custom_diff_tables, BrotliStream& stream)
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

    constexpr hb_tag_t HMTX = HB_TAG('h', 'm', 't', 'x');
    constexpr hb_tag_t VMTX = HB_TAG('v', 'm', 't', 'x');
    constexpr hb_tag_t HHEA = HB_TAG('h', 'h', 'e', 'a');
    constexpr hb_tag_t VHEA = HB_TAG('v', 'h', 'e', 'a');
    constexpr hb_tag_t LOCA = HB_TAG('l', 'o', 'c', 'a');
    constexpr hb_tag_t GLYF = HB_TAG('g', 'l', 'y', 'f');

    hb_tag_t tag = HB_SET_VALUE_INVALID;
    while (hb_set_next(custom_diff_tables, &tag)) {
      switch (tag) {
        case HMTX:
          if (HasTable(base_face, derived_face, HMTX) &&
              HasTable(base_face, derived_face, HHEA)) {
            differs.push_back(RangeAndDiffer(
                base_face, derived_face, HMTX, stream,
                new HmtxDiffer(TableRange::to_span(base_face, HHEA),
                               TableRange::to_span(derived_face, HHEA))));
          }
          break;

        case VMTX:
          if (HasTable(base_face, derived_face, VMTX) &&
              HasTable(base_face, derived_face, VHEA)) {
            differs.push_back(RangeAndDiffer(
                base_face, derived_face, VMTX, stream,
                new HmtxDiffer(TableRange::to_span(base_face, VHEA),
                               TableRange::to_span(derived_face, VHEA))));
          }
          break;

        case LOCA:
          if (HasTable(base_face, derived_face, GLYF) &&
              HasTable(base_face, derived_face, LOCA)) {
            differs.push_back(RangeAndDiffer(base_face, derived_face, LOCA,
                                             stream,
                                             new LocaDiffer(use_short_loca)));
          }
          break;

        case GLYF:
          if (HasTable(base_face, derived_face, GLYF) &&
              HasTable(base_face, derived_face, LOCA)) {
            differs.push_back(RangeAndDiffer(
                base_face, derived_face, GLYF, stream,
                new GlyfDiffer(TableRange::to_span(derived_face, LOCA),
                               use_short_loca)));
          }
          break;
      }
    }
  }

 public:
  std::vector<RangeAndDiffer> differs;

 private:
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

void BrotliFontDiff::SortForDiff(const hb_set_t* immutable_tables,
                                 const hb_set_t* custom_diff_tables,
                                 const hb_face_t* original_face,
                                 hb_face_t* face_builder) {
  // Place generic diff tables,
  // then immutable tables,
  // then custom diff tables.
  std::vector<hb_tag_t> table_order;
  hb_tag_t table_tags[32];
  unsigned offset = 0, num_tables = 32;
  while (((void)hb_face_get_table_tags(original_face, offset, &num_tables,
                                       table_tags),
          num_tables)) {
    for (unsigned i = 0; i < num_tables; ++i) {
      hb_tag_t tag = table_tags[i];
      if (!hb_set_has(immutable_tables, tag) &&
          !hb_set_has(custom_diff_tables, tag)) {
        table_order.push_back(tag);
      }
    }
    offset += num_tables;
  }

  hb_codepoint_t tag = HB_SET_VALUE_INVALID;
  while (hb_set_next(immutable_tables, &tag)) {
    table_order.push_back(tag);
  }

  tag = HB_SET_VALUE_INVALID;
  while (hb_set_next(custom_diff_tables, &tag)) {
    table_order.push_back(tag);
  }

  table_order.push_back(0);

  hb_face_builder_sort_tables(face_builder, table_order.data());
}

StatusCode BrotliFontDiff::Diff(hb_subset_plan_t* base_plan, hb_blob_t* base,
                                hb_subset_plan_t* derived_plan,
                                hb_blob_t* derived, FontData* patch) const {
  Span<const uint8_t> base_span = TableRange::to_span(base);
  Span<const uint8_t> derived_span = TableRange::to_span(derived);

  // get a 'real' (non facebuilder) face for the faces.
  hb_face_t* derived_face = hb_face_create(derived, 0);
  hb_face_t* base_face = hb_face_create(base, 0);

  BrotliStream out(
      BrotliStream::WindowBitsFor(base_span.size(), derived_span.size()),
      base_span.size());

  unsigned derived_start_offset = 0;
  unsigned derived_end_offset = 0;
  unsigned base_start_offset = 0;
  unsigned base_end_offset = 0;

  DiffDriver diff_driver(base_plan, base_face, derived_plan, derived_face,
                         custom_diff_tables_.get(), out);

  hb_tag_t tag = HB_SET_VALUE_INVALID;
  while (hb_set_next(custom_diff_tables_.get(), &tag)) {
    if (!HasTable(derived_face, tag)) {
      continue;
    }

    if (HasTable(base_face, tag) != HasTable(derived_face, tag)) {
      LOG(WARNING) << "base and derived must both have the same tables.";
      return StatusCode::kInternal;
    }

    Span<const uint8_t> base_span =
        TableRange::padded_table_span(TableRange::to_span(base_face, tag));
    Span<const uint8_t> derived_span =
        TableRange::padded_table_span(TableRange::to_span(derived_face, tag));

    unsigned base_offset = TableRange::table_offset(base_face, tag);
    unsigned derived_offset = TableRange::table_offset(derived_face, tag);

    if (!derived_start_offset) {
      derived_start_offset = derived_offset;
    }

    if (!base_start_offset) {
      base_start_offset = base_offset;
    }

    if (derived_end_offset && derived_end_offset != derived_offset) {
      LOG(WARNING) << "custom diff tables in derived are not sequential.";
      return StatusCode::kInternal;
    }

    if (base_end_offset && base_end_offset != base_offset) {
      LOG(WARNING) << "custom diff tables in base are not sequential.";
      return StatusCode::kInternal;
    }

    derived_end_offset = derived_offset + derived_span.size();
    base_end_offset = base_offset + base_span.size();
  }

  out.insert_compressed_with_partial_dict(
      derived_span.subspan(0, derived_start_offset),
      base_span.subspan(0, base_start_offset));

  diff_driver.MakeDiff();

  if (derived_span.size() > derived_end_offset) {
    out.insert_compressed(derived_span.subspan(
        derived_end_offset, derived_end_offset - derived_span.size()));
  }

  out.end_stream();

  patch->copy((const char*)out.compressed_data().data(),
              out.compressed_data().size());

  hb_face_destroy(base_face);
  hb_face_destroy(derived_face);

  return StatusCode::kOk;
}

}  // namespace brotli
