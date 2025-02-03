#include <google/protobuf/text_format.h>

#include <cstdio>
#include <iostream>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/font_data.h"
#include "common/try.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/encoder/glyph_segmentation.h"

/*
 * Given a code point based segmentation creates an appropriate glyph based
 * segmentation and associated activation conditions that maintain the "closure
 * requirement".
 */

ABSL_FLAG(std::string, input_font, "in.ttf",
          "Name of the font to convert to IFT.");

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::make_hb_blob;
using ift::encoder::Encoder;
using ift::encoder::GlyphSegmentation;

StatusOr<FontData> load_file(const char* path) {
  hb_blob_unique_ptr blob =
      make_hb_blob(hb_blob_create_from_file_or_fail(path));
  if (!blob.get()) {
    return absl::NotFoundError(StrCat("File ", path, " was not found."));
  }
  return FontData(blob.get());
}

StatusOr<hb_face_unique_ptr> load_font(const char* filename) {
  return TRY(load_file(filename)).face();
}

constexpr uint32_t NETWORK_REQUEST_BYTE_OVERHEAD = 75;

StatusOr<int> EncodingSize(const Encoder::Encoding& encoding) {
  auto init_font = encoding.init_font.face();
  
  uint32_t total_size = 0;
  for (const auto& [url, data] : encoding.patches) {
    if (url.substr(url.size() - 2) == "gk") {
      total_size += data.size() + NETWORK_REQUEST_BYTE_OVERHEAD;
      printf("  patch %s adds %u bytes, %u bytes overhead\n", url.c_str(), data.size(), NETWORK_REQUEST_BYTE_OVERHEAD);
    }
  }

  auto iftx = FontHelper::TableData(init_font.get(), HB_TAG('I', 'F', 'T', 'X'));
  total_size += iftx.size();
  printf("  mapping table %u bytes\n", iftx.size());

  return total_size;
}

StatusOr<int> IdealSegmentationSize(hb_face_t* font, const GlyphSegmentation& segmentation, uint32_t number_input_segments) {
  // There are three parts to the cost of a segmentation:
  // - Size of the glyph keyed mapping table.
  // - Total size of all glyph keyed patches
  // - Network overhead (fixed cost per patch).

  printf("IdealSegmentationSize():\n");
  btree_set<uint32_t> glyphs;
  for (const auto& [id, glyph_set] : segmentation.GidSegments()) {
    glyphs.insert(glyph_set.begin(), glyph_set.end());
  }

  uint32_t glyphs_per_patch = glyphs.size() / number_input_segments;
  uint32_t remainder_glyphs = glyphs.size() % number_input_segments;


  Encoder encoder;
  encoder.SetFace(font);

  flat_hash_set<uint32_t> all_segments;

  TRYV(encoder.SetBaseSubset({}));

  auto glyphs_it = glyphs.begin();  
  for (uint32_t i = 0; i < number_input_segments; i++) {
    auto begin = glyphs_it;
    glyphs_it  = std::next(glyphs_it, glyphs_per_patch);
    if (remainder_glyphs > 0) {
      glyphs_it++;
      remainder_glyphs--;
    }

    flat_hash_set<uint32_t> gids;
    gids.insert(begin, glyphs_it);
    TRYV(encoder.AddGlyphDataSegment(i, gids));
    all_segments.insert(i);
    TRYV(encoder.AddGlyphDataActivationCondition(Encoder::Condition(i)));
  }

  TRYV(encoder.AddNonGlyphSegmentFromGlyphSegments(all_segments));

  auto encoding = TRY(encoder.Encode());
  return EncodingSize(encoding);
}

StatusOr<int> SegmentationSize(hb_face_t* font, const GlyphSegmentation& segmentation) {
  // There are three parts to the cost of a segmentation:
  // - Size of the glyph keyed mapping table.
  // - Total size of all glyph keyed patches
  // - Network overhead (fixed cost per patch).
  printf("SegmentationSize():\n");
  Encoder encoder;
  encoder.SetFace(font);

  flat_hash_set<uint32_t> all_segments;

  TRYV(encoder.SetBaseSubset({}));

  for (const auto& [id, glyph_set] : segmentation.GidSegments()) {
    flat_hash_set<uint32_t> s;
    s.insert(glyph_set.begin(), glyph_set.end());
    TRYV(encoder.AddGlyphDataSegment(id, s));
    all_segments.insert(id);
  }

  TRYV(encoder.AddNonGlyphSegmentFromGlyphSegments(all_segments));

  for (const auto& c : segmentation.Conditions()) {
    Encoder::Condition condition;
    for (const auto& g : c.conditions()) {
      condition.required_groups.push_back(g);
    }
    condition.activated_segment_id = c.activated();
    TRYV(encoder.AddGlyphDataActivationCondition(condition));
  }

  auto encoding = TRY(encoder.Encode());
  return EncodingSize(encoding);
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  auto font = load_font(absl::GetFlag(FLAGS_input_font).c_str());
  if (!font.ok()) {
    std::cerr << "Failed to load input font: " << font.status() << std::endl;
    return -1;
  }

  auto result = ift::encoder::GlyphSegmentation::CodepointToGlyphSegments(
      font->get(), {'a', 'b'}, {{'c', 'd', 'f'}, {'i'}});
  if (!result.ok()) {
    std::cerr << result.status() << std::endl;
    return -1;
  }

  std::cout << ">> Computed Segmentation" << std::endl;
  std::cout << result->ToString() << std::endl;

  std::cout << ">> Analysis" << std::endl;  
  auto cost = SegmentationSize(font->get(), *result);
  if (!cost.ok()) {
    std::cerr << "Failed to compute segmentation cost: " << cost.status() << std::endl;
  }
  auto ideal_cost = IdealSegmentationSize(font->get(), *result, 2);
  if (!ideal_cost.ok()) {
    std::cerr << "Failed to compute segmentation cost: " << cost.status() << std::endl;
  }

  std::cout << std::endl;
  std::cout << "glyphs_in_fallback = "
            << result->UnmappedGlyphs().size() << std::endl;
  std::cout << "ideal_cost_bytes = " << *ideal_cost << std::endl;
  std::cout << "total_cost_bytes = " << *cost << std::endl;  

  double overhead_percent = (((double) *cost) / ((double) *ideal_cost) * 100.0) - 100.0;
  std::cout << "%_overhead = " << overhead_percent << std::endl;

  return 0;
}
