#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/brotli_binary_patch.h"
#include "hb-subset.h"

#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

using namespace std::chrono;
using std::string_view;
using std::vector;

using patch_subset::BrotliBinaryDiff;
using patch_subset::BrotliBinaryPatch;
using patch_subset::FontData;
using patch_subset::StatusCode;

constexpr bool DUMP_STATE = false;
constexpr unsigned STATIC_QUALITY = 11;
// Number of codepoints to include in the subset. Set to
// -1 to use ascii as a subset.
constexpr unsigned SUBSET_COUNT = 1000;
constexpr unsigned BASE_COUNT = 750;
constexpr unsigned TRIAL_DURATION_MS = 500;

enum Mode {
  PRECOMPRESS_LAYOUT = 0,
  IMMUTABLE_LAYOUT,
  MUTABLE_LAYOUT,
  END,
};

// TODO(grieger): this should be all "No Subset Tables" in the font.
hb_tag_t immutable_tables[] = {
  HB_TAG ('G', 'D', 'E', 'F'),
  HB_TAG ('G', 'S', 'U', 'B'),
  HB_TAG ('G', 'P', 'O', 'S'),
  0,
};

void dump(const char* name, const char* data, unsigned size)
{
  FILE* f = fopen(name, "w");
  fwrite(data, size, 1, f);
  fclose(f);
}

unsigned table_directory_size(const hb_face_t* face)
{
  unsigned num_tables = hb_face_get_table_tags (face, 0, nullptr, nullptr);
  return 12 + num_tables * 16;
}

vector<uint8_t> precompress_immutable(const hb_face_t* face)
{
  vector<uint8_t> table_data;
  for (hb_tag_t* tag = immutable_tables;
       *tag;
       tag++)
  {
    hb_blob_t* blob = hb_face_reference_table(face, *tag);
    unsigned int length = 0;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
    table_data.insert(table_data.end(), data, data + length);
    while (table_data.size () % 4 != 0) { // Pad to 4 byte boundary.
      table_data.push_back (0);
    }
    hb_blob_destroy (blob);
  }

  unsigned header_size = table_directory_size (face);

  vector<uint8_t> sink;
  FontData empty;
  BrotliBinaryDiff differ(STATIC_QUALITY);
  StatusCode sc = differ.Diff(empty,
                              std::string_view(reinterpret_cast<const char*>(table_data.data()),
                                               table_data.size()),
                              header_size,
                              false,
                              sink);
  assert(sc == StatusCode::kOk);

  return sink;
}

hb_face_t* make_subset (hb_face_t* face, hb_set_t* codepoints, Mode mode)
{
  hb_face_t* subset = nullptr;

  hb_subset_input_t* input = hb_subset_input_create_or_fail ();

  hb_set_clear (hb_subset_input_set (input, HB_SUBSET_SETS_DROP_TABLE_TAG));
  hb_set_union (hb_subset_input_unicode_set (input), codepoints);

  if (mode != MUTABLE_LAYOUT) {
    for (hb_tag_t* tag = immutable_tables;
         *tag;
         tag++)
      hb_set_add (hb_subset_input_set (input, HB_SUBSET_SETS_NO_SUBSET_TABLE_TAG),
                  *tag);
    hb_subset_input_set_flags (input,
                               HB_SUBSET_FLAGS_RETAIN_GIDS | HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
  } else {
    hb_subset_input_set_flags (input, HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
  }

  subset = hb_subset_or_fail (face, input);
  hb_subset_input_destroy (input);

  // Reorder immutable tables to be first.
  if (mode != MUTABLE_LAYOUT) {
    hb_face_builder_set_table_ordering (subset, immutable_tables);
  }

  return subset;
}

void add_compressed_table_directory(const hb_face_t* face,
                                    hb_blob_t* subset_blob,
                                    vector<uint8_t>& patch)
{
  // There's a high overhead to brotli compressing the small table directory at the front and
  // compression here only saves a small number of bytes. So instead just emit a uncompressed
  // literal meta-block. We manually construct the appropriate stream and meta block header.

  // stream + meta-block header is 4 bytes total:
  // WINDOW  ISLAST  MNIBBLES (4)  MLEN-1             ISUNCOMPRESSED PAD (5 bits)
  // 1000000 0       00            XXXXXXXX XXXXXXXX  1              00000
  //
  // Example (size = 299 (00000001 00101011))
  //                1        172      4        4
  // Encoded as: 00000001 10101100 00000100 00000100
  unsigned window_bits = 1; // 17 (0000001)
  // TODO(grieger): compute based on size of # of tables in the subset, then we can re-enable
  //                the standard drop tables list.
  unsigned size = table_directory_size(face);
  unsigned mlen = size - 1;
  uint8_t encoded_size[2] = {
    (uint8_t) (mlen & 0xFF),
    (uint8_t) ((mlen >> 8) & 0xFF),
  };

  uint32_t header =
      (window_bits & 0b1111111) | // Window Bits
      (encoded_size[0] << 10)   | // MLEN
      (encoded_size[1]  << 18)  | // MLEN
      (1 << 26);                  // ISUNCOMPRESSED
  uint8_t encoded_header[4] = {
    (uint8_t) (header & 0xFF),
    (uint8_t) (header >> 8 & 0xFF),
    (uint8_t) (header >> 16 & 0xFF),
    (uint8_t) (header >> 24 & 0xFF),
  };

  patch.push_back(encoded_header[0]);
  patch.push_back(encoded_header[1]);
  patch.push_back(encoded_header[2]);
  patch.push_back(encoded_header[3]);

  string_view table_directory = string_view(hb_blob_get_data(subset_blob, nullptr),
                                            size);
  patch.insert(patch.end(), table_directory.begin(), table_directory.end());
}

void add_mutable_tables (const FontData& base,
                         hb_blob_t* blob,
                         unsigned quality,
                         unsigned offset,
                         vector<uint8_t>& patch)
{
  BrotliBinaryDiff differ(quality);
  differ.Diff(base,
              string_view(hb_blob_get_data(blob, nullptr) + offset,
                          hb_blob_get_length(blob) - offset),
              offset,
              true,
              patch);
}

unsigned table_length(const hb_face_t* face, hb_tag_t tag)
{
  unsigned length = hb_blob_get_length(hb_face_reference_table(face, tag));
  while (length % 4) { // Pad to 4 byte boundary.
    length++;
  }
  return length;
}

unsigned precompressed_length(const hb_face_t* face)
{
  unsigned total = 0;
  for (hb_tag_t* tag = immutable_tables;
       *tag;
       tag++) {
    total += table_length(face, *tag);
  }
  return total;
}

unsigned make_patch (hb_face_t* face,
                     hb_set_t* base_codepoints,
                     hb_set_t* subset_codepoints,
                     Mode mode,
                     unsigned dynamic_quality,
                     unsigned codepoint_count,
                     unsigned i)
{
  static const vector<uint8_t> precompressed = precompress_immutable(face);

  FontData base;
  if (hb_set_get_population (base_codepoints) > 0) {
    hb_face_t* subset = make_subset (face, base_codepoints, mode);
    hb_blob_t* blob = hb_face_reference_blob (subset);
    base.copy (hb_blob_get_data (blob, nullptr), hb_blob_get_length (blob));
    hb_face_destroy (subset);
    hb_blob_destroy (blob);
  }


  hb_face_t* subset = make_subset (face, subset_codepoints, mode);
  hb_blob_t* blob = hb_face_reference_blob (subset);

  vector<uint8_t> patch;

  if (hb_set_get_population (base_codepoints) == 0 && mode == PRECOMPRESS_LAYOUT) {
    add_compressed_table_directory(face, blob, patch);
    patch.insert(patch.end(), precompressed.begin(), precompressed.end());
    add_mutable_tables(base, blob, dynamic_quality, table_directory_size(face) + precompressed_length(face), patch);
  } else {
    add_mutable_tables(base, blob, dynamic_quality, 0, patch);
  }

  FontData font_patch;
  font_patch.copy(reinterpret_cast<const char*>(patch.data()),
                  patch.size());
  BrotliBinaryPatch patcher;


  if (i == 0) {

    FontData derived;
    StatusCode sc = patcher.Patch(base, font_patch, &derived);
    if (sc != StatusCode::kOk) {
      printf("Patch application failed.\n");
      exit(-1);
    }
    if (DUMP_STATE) {
      dump("patch.bin", reinterpret_cast<const char*>(patch.data()), patch.size());
      dump("actual_subset.ttf", hb_blob_get_data(blob, nullptr), hb_blob_get_length(blob));
      dump("generated_subset.ttf", reinterpret_cast<const char*>(derived.data()), derived.size());
    }

    if (derived.size() != hb_blob_get_length(blob)
        || memcmp(reinterpret_cast<const void*>(derived.data()),
                  reinterpret_cast<const void*>(hb_blob_get_data(blob, nullptr)),
                  derived.size())) {
      printf("Derived subset is not equivalent to expected subset.\n");
      exit(-1);
    }
  }

  hb_blob_destroy (blob);

  return patch.size();
}

const char* mode_to_string(Mode mode) {
  switch (mode) {
    case PRECOMPRESS_LAYOUT:
      return "PRECOMPRESS_LAYOUT";
    case IMMUTABLE_LAYOUT:
      return "IMMUTABLE_LAYOUT";
    case MUTABLE_LAYOUT:
      return "MUTABLE_LAYOUT";
    default:
      return "UNKNOWN";
  }
}

void create_subset_set (hb_face_t* face, hb_set_t* codepoints, unsigned count)
{
  if (count == (unsigned) -1) {
    // ASCII
    hb_set_add_range (codepoints, 0, 255);
    return;
  }

  hb_set_t* all_codepoints = hb_set_create ();
  hb_face_collect_unicodes (face, all_codepoints);

  unsigned i = 0;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next (all_codepoints, &cp) && i < count) {
    hb_set_add (codepoints, cp);
    i++;
  }

  hb_set_destroy (all_codepoints);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "ERROR: invalid args." << std::endl;
    return -1;
  }

  char* font_path = argv[1];

  hb_blob_t* font_blob = hb_blob_create_from_file_or_fail (font_path);
  if (!font_blob) {
    std::cout << "ERROR: invalid file path." << std::endl;
    return -1;
  }

  hb_face_t* face = hb_face_create (font_blob, 0);
  hb_set_t* base_codepoints = hb_set_create ();
  hb_set_t* subset_codepoints = hb_set_create ();
  create_subset_set (face, base_codepoints, BASE_COUNT);
  create_subset_set (face, subset_codepoints, SUBSET_COUNT);

  hb_blob_destroy (font_blob);

  printf("mode, quality, duration_ms, iterations, patch_size, ms/req\n");
  unsigned start_mode = BASE_COUNT > 0 ? IMMUTABLE_LAYOUT : PRECOMPRESS_LAYOUT;
  for (unsigned mode = start_mode; mode < END; mode++) {
    for (unsigned quality = 0; quality <= 9; quality ++) {
      unsigned patch_size = 0;
      unsigned duration_ms = 0;
      auto start = high_resolution_clock::now();

      unsigned i = 0;
      while (true) {
        patch_size = make_patch (face,
                                 base_codepoints,
                                 subset_codepoints,
                                 (Mode) mode, quality, 0, i);
        if (i % 20 == 0) {
          auto stop = high_resolution_clock::now();
          duration_ms = duration_cast<milliseconds>(stop - start).count();
          if (duration_ms > TRIAL_DURATION_MS) {
            break;
          }
        }
        i++;
      }

      float ms_per_request = (float) duration_ms / (float) i;
      printf("%s, %u, %u, %u, %u, %.2f\n",
             mode_to_string((Mode) mode), quality, duration_ms, i, patch_size, ms_per_request);
    }
  }

  hb_face_destroy (face);
  hb_set_destroy (base_codepoints);
  hb_set_destroy (subset_codepoints);
  return 0;
}
