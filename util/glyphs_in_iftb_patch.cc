#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>

#include "absl/flags/parse.h"
#include "common/font_data.h"
#include "ift/glyph_keyed_diff.h"

/*
 * This utility dumps the list of gids covered by an IFTB patch.
 */

using common::FontData;
using ift::GlyphKeyedDiff;

FontData load_patch(const char* filename) {
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", filename);
    exit(-1);
  }
  FontData result(blob);
  return result;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() != 2) {
    std::cout << "This utility dumps the list of gids covered by an IFTB patch."
              << std::endl;
    std::cout << "usage: glyphs_in_iftb_patch "
                 "<input font>\n";
    return -1;
  }

  auto patch = load_patch(args[1]);
  auto gids = GlyphKeyedDiff::GidsInIftbPatch(patch);
  if (!gids.ok()) {
    std::cerr << "Parsing input patch failed: " << gids.status() << std::endl;
    return -1;
  }

  absl::btree_set<uint32_t> sorted_gids;
  for (auto gid : *gids) {
    sorted_gids.insert(gid);
  }

  for (auto gid : sorted_gids) {
    std::cout << gid << ", ";
  }

  return 0;
}