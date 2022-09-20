#include "patch_subset/brotli_binary_diff.h"
#include "hb-subset.h"


#include <iostream>
#include <string>
#include <vector>

using std::string_view;
using std::vector;

using patch_subset::BrotliBinaryDiff;
using patch_subset::FontData;
using patch_subset::StatusCode;

hb_tag_t immutable_tables[] = {
  HB_TAG ('G', 'D', 'E', 'F'),
  HB_TAG ('G', 'S', 'U', 'B'),
  HB_TAG ('G', 'P', 'O', 'S'),
  0,
};

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
    hb_blob_destroy (blob);
  }

  unsigned header_size = table_directory_size (face);

  vector<uint8_t> sink;
  FontData empty;
  BrotliBinaryDiff differ;
  StatusCode sc = differ.Diff(empty,
                              std::string_view(reinterpret_cast<const char*>(table_data.data()),
                                               table_data.size()),
                              header_size,
                              false,
                              sink);
  assert(sc == StatusCode::kOk);

  return sink;
}

hb_face_t* make_subset (hb_face_t* face)
{
  hb_face_t* subset = nullptr;

  hb_subset_input_t* input = hb_subset_input_create_or_fail ();

  hb_set_add_range (hb_subset_input_unicode_set (input),
                    'a', 'z');

  for (hb_tag_t* tag = immutable_tables;
       *tag;
       tag++)
    hb_set_add (hb_subset_input_set (input, HB_SUBSET_SETS_NO_SUBSET_TABLE_TAG),
                *tag);
  hb_subset_input_set_flags (input, HB_SUBSET_FLAGS_RETAIN_GIDS);

  subset = hb_subset_or_fail (face, input);
  hb_subset_input_destroy (input);

  // Reorder immutable tables to be first.
  unsigned num_tables = hb_face_get_table_tags (face, 0, nullptr, nullptr);
  vector<hb_tag_t> tags;
  tags.resize (num_tables);
  hb_face_get_table_tags (face, 0, &num_tables, tags.data());
  for (hb_tag_t tag : tags) {
    assert (hb_face_builder_set_table_order (subset, tag, 100));
  }

  unsigned i = 0;
  for (hb_tag_t* tag = immutable_tables;
       *tag;
       tag++) {
    assert (hb_face_builder_set_table_order (subset, *tag, i++));
  }

  return subset;
}

void add_compressed_table_directory(const hb_face_t* subset,
                                    hb_blob_t* blob,
                                    vector<uint8_t>& patch)
{
  unsigned size = table_directory_size(subset);

  BrotliBinaryDiff differ;
  FontData empty;

  differ.Diff(empty,
              string_view(hb_blob_get_data(blob, nullptr),
                          size),
              0,
              false,
              patch);
}

void add_mutable_tables (hb_blob_t* blob,
                         unsigned offset,
                         vector<uint8_t>& patch)
{
  BrotliBinaryDiff differ;
  FontData empty;

  differ.Diff(empty,
              string_view(hb_blob_get_data(blob, nullptr) + offset,
                          hb_blob_get_length(blob) - offset),
              offset,
              true,
              patch);
}

unsigned table_length(const hb_face_t* face, hb_tag_t tag)
{
  return hb_blob_get_length(hb_face_reference_table(face, tag));
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

void make_patch (hb_face_t* face, const vector<uint8_t>& precompressed)
{
  hb_face_t* subset = make_subset (face);
  hb_blob_t* blob = hb_face_reference_blob (subset);

  vector<uint8_t> patch;
  add_compressed_table_directory(subset, blob, patch);
  printf("1. patch_size = %lu\n", patch.size());

  patch.insert(patch.end(), precompressed.begin(), precompressed.end());
  printf("2. patch_size = %lu\n", patch.size());

  add_mutable_tables(blob, table_directory_size(subset) + precompressed_length(face), patch);
  printf("3. patch_size = %lu\n", patch.size());

  // TODO(grieger): test patch.

  hb_blob_destroy (blob);
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
  hb_blob_destroy (font_blob);

  vector<uint8_t> precompressed = precompress_immutable(face);

  for (unsigned i = 0; i < 1; i++)
    make_patch (face, precompressed);


  hb_face_destroy (face);
  return 0;
}
