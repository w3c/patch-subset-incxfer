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

  std::cout << result->ToString();

  std::cout << std::endl;
  std::cout << "Number of glyphs placed in fallback patch: "
            << result->UnmappedGlyphs().size() << std::endl;

  return 0;
}
