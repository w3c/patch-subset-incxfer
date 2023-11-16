#include <stdio.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "brotli/brotli_font_diff.h"
#include "common/brotli_binary_diff.h"
#include "common/brotli_binary_patch.h"
#include "common/hb_set_unique_ptr.h"
#include "hb-subset.h"

using namespace std::chrono;
using std::vector;

using absl::Span;
using absl::Status;
using absl::string_view;
using brotli::BrotliFontDiff;
using common::BrotliBinaryDiff;
using common::BrotliBinaryPatch;
using common::FontData;
using common::hb_set_unique_ptr;
using common::make_hb_set;

constexpr bool DUMP_STATE = false;
constexpr unsigned STATIC_QUALITY = 11;
// Number of codepoints to include in the subset. Set to
// -1 to use ascii as a subset.
constexpr unsigned SUBSET_COUNT = 10;
constexpr unsigned BASE_COUNT = 1000;
constexpr unsigned TRIAL_DURATION_MS = 5000;

// TODO(grieger): this should be all "No Subset Tables" in the font.
hb_set_unique_ptr immutable_tables_set =
    make_hb_set(3, HB_TAG('G', 'D', 'E', 'F'), HB_TAG('G', 'S', 'U', 'B'),
                HB_TAG('G', 'P', 'O', 'S'));

hb_set_unique_ptr custom_tables_set =
    make_hb_set(4, HB_TAG('g', 'l', 'y', 'f'), HB_TAG('l', 'o', 'c', 'a'),
                HB_TAG('h', 'm', 't', 'x'), HB_TAG('v', 'm', 't', 'x'));

static void dump(const char* name, const char* data, unsigned size) {
  FILE* f = fopen(name, "w");
  fwrite(data, size, 1, f);
  fclose(f);
}

static unsigned table_directory_size(const hb_face_t* face) {
  unsigned num_tables = hb_face_get_table_tags(face, 0, nullptr, nullptr);
  return 12 + num_tables * 16;
}

enum Mode {
  PRECOMPRESS_LAYOUT = 0,
  IMMUTABLE_LAYOUT,
  MUTABLE_LAYOUT,
  CUSTOM_DIFF,
  CUSTOM_DIFF_IMMUTABLE_LAYOUT,
  END,
};

static bool IsCustomDiff(Mode mode) {
  return mode == CUSTOM_DIFF || mode == CUSTOM_DIFF_IMMUTABLE_LAYOUT;
}

static bool IsLayoutImmutable(Mode mode) {
  return mode == PRECOMPRESS_LAYOUT || mode == IMMUTABLE_LAYOUT ||
         mode == CUSTOM_DIFF_IMMUTABLE_LAYOUT;
}

class Operation {
 public:
  Operation(hb_blob_t* original_, hb_set_t* base_set_, hb_set_t* subset_set_)
      : original(original_), base_set(base_set_), subset_set(subset_set_) {}

  ~Operation() {
    hb_set_destroy(base_set);
    hb_set_destroy(subset_set);
    hb_blob_destroy(original);
    hb_blob_destroy(base);
    hb_blob_destroy(subset);

    hb_face_destroy(original_face);
    hb_face_destroy(base_face);
    hb_face_destroy(subset_face);

    hb_subset_plan_destroy(base_plan);
    hb_subset_plan_destroy(subset_plan);
  }

  unsigned MakeSubsets() {
    original_face = hb_face_create(original, 0);

    unsigned base_size = 0;
    if (hb_set_get_population(base_set) > 0) {
      base = make_subset(original_face, base_set, &base_plan);
      base_face = hb_face_create(base, 0);
      base_size = hb_blob_get_length(base);
    }

    subset = make_subset(original_face, subset_set, &subset_plan);
    subset_face = hb_face_create(subset, 0);

    return base_size;
  }

  unsigned MakePatch(unsigned i) {
    vector<uint8_t> patch;
    if (!base && mode == PRECOMPRESS_LAYOUT) {
      add_compressed_table_directory(subset_face, subset, patch);
      patch.insert(patch.end(), precompressed.begin(), precompressed.end());
      Status sc = add_mutable_tables(
          table_directory_size(subset_face) + precompressed_length(subset_face),
          patch);
      if (!sc.ok()) {
        std::cout << "Adding tables failed (1): " << sc << std::endl;
        exit(-1);
      }
    } else if (IsCustomDiff(mode)) {
      // Use custom differ
      hb_set_unique_ptr empty_set = make_hb_set();
      hb_set_t* immutable_tables = IsLayoutImmutable(mode)
                                       ? immutable_tables_set.get()
                                       : empty_set.get();
      BrotliFontDiff differ(immutable_tables, custom_tables_set.get());
      FontData patch_data;
      Status sc =
          differ.Diff(base_plan, base, subset_plan, subset, &patch_data);
      if (!sc.ok()) {
        std::cout << "Patch diff generation failed: " << sc << std::endl;
        exit(-1);
      }
      patch.insert(patch.end(), patch_data.str().begin(),
                   patch_data.str().end());
    } else {
      Status sc = add_mutable_tables(0, patch);
      if (!sc.ok()) {
        std::cout << "Adding tables failed (2): " << sc << std::endl;
        exit(-1);
      }
    }

    if (i == 0) {
      FontData font_patch;
      font_patch.copy(reinterpret_cast<const char*>(patch.data()),
                      patch.size());
      BrotliBinaryPatch patcher;

      FontData base_font_data;
      if (base) {
        base_font_data.set(base);
      }
      if (DUMP_STATE) {
        dump("actual_subset.ttf", hb_blob_get_data(subset, nullptr),
             hb_blob_get_length(subset));
      }

      FontData derived;
      Status sc = patcher.Patch(base_font_data, font_patch, &derived);
      if (!sc.ok()) {
        std::cout << "Patch application failed: " << sc << std::endl;
        exit(-1);
      }
      if (DUMP_STATE) {
        dump("patch.bin", reinterpret_cast<const char*>(patch.data()),
             patch.size());
        dump("generated_subset.ttf",
             reinterpret_cast<const char*>(derived.data()), derived.size());
      }

      if (derived.size() != hb_blob_get_length(subset) ||
          memcmp(
              reinterpret_cast<const void*>(derived.data()),
              reinterpret_cast<const void*>(hb_blob_get_data(subset, nullptr)),
              derived.size())) {
        printf("Derived subset is not equivalent to expected subset.\n");
        exit(-1);
      }
    }

    return patch.size();
  }

 private:
  unsigned table_length(const hb_face_t* face, hb_tag_t tag) {
    unsigned length = hb_blob_get_length(hb_face_reference_table(face, tag));
    while (length % 4) {  // Pad to 4 byte boundary.
      length++;
    }
    return length;
  }

  unsigned precompressed_length(const hb_face_t* face) {
    unsigned total = 0;
    hb_tag_t tag = HB_SET_VALUE_INVALID;
    while (hb_set_next(immutable_tables_set.get(), &tag)) {
      total += table_length(face, tag);
    }
    return total;
  }

  void add_compressed_table_directory(const hb_face_t* face,
                                      hb_blob_t* subset_blob,
                                      vector<uint8_t>& patch) {
    // There's a high overhead to brotli compressing the small table directory
    // at the front and compression here only saves a small number of bytes. So
    // instead just emit a uncompressed literal meta-block. We manually
    // construct the appropriate stream and meta block header.

    // stream + meta-block header is 4 bytes total:
    // WINDOW  ISLAST  MNIBBLES (4)  MLEN-1             ISUNCOMPRESSED PAD (5
    // bits) 1000000 0       00            XXXXXXXX XXXXXXXX  1 00000
    //
    // Example (size = 299 (00000001 00101011))
    //                1        172      4        4
    // Encoded as: 00000001 10101100 00000100 00000100
    unsigned window_bits = 1;  // 17 (0000001)
    // TODO(grieger): compute based on size of # of tables in the subset, then
    // we can re-enable
    //                the standard drop tables list.
    unsigned size = table_directory_size(face);
    unsigned mlen = size - 1;
    uint8_t encoded_size[2] = {
        (uint8_t)(mlen & 0xFF),
        (uint8_t)((mlen >> 8) & 0xFF),
    };

    uint32_t header = (window_bits & 0b1111111) |  // Window Bits
                      (encoded_size[0] << 10) |    // MLEN
                      (encoded_size[1] << 18) |    // MLEN
                      (1 << 26);                   // ISUNCOMPRESSED
    uint8_t encoded_header[4] = {
        (uint8_t)(header & 0xFF),
        (uint8_t)(header >> 8 & 0xFF),
        (uint8_t)(header >> 16 & 0xFF),
        (uint8_t)(header >> 24 & 0xFF),
    };

    patch.push_back(encoded_header[0]);
    patch.push_back(encoded_header[1]);
    patch.push_back(encoded_header[2]);
    patch.push_back(encoded_header[3]);

    string_view table_directory =
        string_view(hb_blob_get_data(subset_blob, nullptr), size);
    patch.insert(patch.end(), table_directory.begin(), table_directory.end());
  }

  hb_blob_t* make_subset(hb_face_t* face, hb_set_t* codepoints,
                         hb_subset_plan_t** plan) {
    hb_face_t* subset = nullptr;

    hb_subset_input_t* input = hb_subset_input_create_or_fail();

    hb_set_clear(hb_subset_input_set(input, HB_SUBSET_SETS_DROP_TABLE_TAG));
    hb_set_union(hb_subset_input_unicode_set(input), codepoints);

    if (IsLayoutImmutable(mode)) {
      hb_tag_t tag = HB_SET_VALUE_INVALID;
      while (hb_set_next(immutable_tables_set.get(), &tag)) {
        hb_set_add(
            hb_subset_input_set(input, HB_SUBSET_SETS_NO_SUBSET_TABLE_TAG),
            tag);
      }
      hb_subset_input_set_flags(input,
                                HB_SUBSET_FLAGS_RETAIN_GIDS |
                                    HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
    } else {
      hb_subset_input_set_flags(input,
                                HB_SUBSET_FLAGS_PASSTHROUGH_UNRECOGNIZED);
    }

    *plan = hb_subset_plan_create_or_fail(face, input);
    subset = hb_subset_plan_execute_or_fail(*plan);
    hb_subset_input_destroy(input);

    // Re-order font tables if so requried by the mode
    if (IsCustomDiff(mode)) {
      BrotliFontDiff::SortForDiff(immutable_tables_set.get(),
                                  custom_tables_set.get(), face, subset);
    } else if (IsLayoutImmutable(mode)) {
      std::vector<hb_tag_t> immutable_tables;
      hb_tag_t tag = HB_SET_VALUE_INVALID;
      while (hb_set_next(immutable_tables_set.get(), &tag)) {
        immutable_tables.push_back(tag);
      }
      immutable_tables.push_back(0);
      hb_face_builder_sort_tables(subset, immutable_tables.data());
    }

    hb_blob_t* result = hb_face_reference_blob(subset);
    hb_face_destroy(subset);

    return result;
  }

  Status add_mutable_tables(unsigned offset, vector<uint8_t>& patch) {
    BrotliBinaryDiff differ(dynamic_quality);
    FontData base_font_data;
    if (base) {
      base_font_data.set(base);
    }

    return differ.Diff(base_font_data,
                       string_view(hb_blob_get_data(subset, nullptr) + offset,
                                   hb_blob_get_length(subset) - offset),
                       offset, true, patch);
  }

 private:
  hb_blob_t* original;
  hb_set_t* base_set;
  hb_set_t* subset_set;

  hb_blob_t* base = nullptr;
  hb_blob_t* subset = nullptr;

  hb_face_t* original_face = nullptr;
  hb_face_t* base_face = nullptr;
  hb_face_t* subset_face = nullptr;

  hb_subset_plan_t* base_plan = nullptr;
  hb_subset_plan_t* subset_plan = nullptr;

 public:
  Mode mode = MUTABLE_LAYOUT;
  unsigned dynamic_quality = 5;
  Span<const uint8_t> precompressed;
};

vector<uint8_t> precompress_immutable(const hb_face_t* face) {
  vector<uint8_t> table_data;
  hb_tag_t tag = HB_SET_VALUE_INVALID;
  while (hb_set_next(immutable_tables_set.get(), &tag)) {
    hb_blob_t* blob = hb_face_reference_table(face, tag);
    unsigned int length = 0;
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
    table_data.insert(table_data.end(), data, data + length);
    while (table_data.size() % 4 != 0) {  // Pad to 4 byte boundary.
      table_data.push_back(0);
    }
    hb_blob_destroy(blob);
  }

  unsigned header_size = table_directory_size(face);

  vector<uint8_t> sink;
  FontData empty;
  BrotliBinaryDiff differ(STATIC_QUALITY);
  Status sc =
      differ.Diff(empty,
                  string_view(reinterpret_cast<const char*>(table_data.data()),
                              table_data.size()),
                  header_size, false, sink);
  if (!sc.ok()) {
    std::cout << "Precompression brotli encoding failed: " << sc << std::endl;
    exit(-1);
  }

  return sink;
}

Status add_mutable_tables(hb_blob_t* base, hb_blob_t* subset, unsigned quality,
                          unsigned offset, vector<uint8_t>& patch) {
  FontData base_data(base);
  BrotliBinaryDiff differ(quality);
  return differ.Diff(base_data,
                     string_view(hb_blob_get_data(subset, nullptr) + offset,
                                 hb_blob_get_length(subset) - offset),
                     offset, true, patch);
}

const char* mode_to_string(Mode mode) {
  switch (mode) {
    case PRECOMPRESS_LAYOUT:
      return "PRECOMPRESS_LAYOUT";
    case IMMUTABLE_LAYOUT:
      return "IMMUTABLE_LAYOUT";
    case MUTABLE_LAYOUT:
      return "MUTABLE_LAYOUT";
    case CUSTOM_DIFF:
      return "CUSTOM_DIFF";
    case CUSTOM_DIFF_IMMUTABLE_LAYOUT:
      return "CUSTOM_DIFF_IMMUTABLE_LAYOUT";
    default:
      return "UNKNOWN";
  }
}

void create_subset_set(hb_face_t* face, hb_set_t* codepoints, unsigned count) {
  if (count == (unsigned)-1) {
    // ASCII
    hb_set_add_range(codepoints, 0, 255);
    return;
  }

  hb_set_t* all_codepoints = hb_set_create();
  hb_face_collect_unicodes(face, all_codepoints);

  unsigned i = 0;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(all_codepoints, &cp) && i < count) {
    hb_set_add(codepoints, cp);
    i++;
  }

  hb_set_destroy(all_codepoints);
}

void test_dictionary_size(hb_face_t* face) {
  printf(
      "quality, duration_ms, iterations, base_codepoints, base_size, "
      "patch_size, ms/req\n");
  unsigned quality = 5;
  for (unsigned base_count = BASE_COUNT; base_count <= 10 * BASE_COUNT;
       base_count += BASE_COUNT) {
    hb_set_t* base_codepoints = hb_set_create();
    hb_set_t* subset_codepoints = hb_set_create();
    create_subset_set(face, base_codepoints, base_count);
    create_subset_set(face, subset_codepoints, base_count + SUBSET_COUNT);

    unsigned patch_size = 0;
    unsigned duration_ms = 0;

    Operation op(hb_face_reference_blob(face), base_codepoints,
                 subset_codepoints);
    op.dynamic_quality = 5;
    op.mode = CUSTOM_DIFF;
    unsigned base_size = op.MakeSubsets();

    auto start = high_resolution_clock::now();

    unsigned i = 0;
    while (true) {
      patch_size = op.MakePatch(i);

      if (i % 20 == 0) {
        auto stop = high_resolution_clock::now();
        duration_ms = duration_cast<milliseconds>(stop - start).count();
        if (duration_ms > TRIAL_DURATION_MS) {
          break;
        }
      }
      i++;
    }

    float ms_per_request = (float)duration_ms / (float)i;
    printf("%u, %u, %u, %u, %u, %u, %.2f\n", quality, duration_ms, i,
           base_count, base_size, patch_size, ms_per_request);
  }
}

void test_precompression(hb_face_t* face) {
  hb_set_t* base_codepoints = hb_set_create();
  hb_set_t* subset_codepoints = hb_set_create();
  create_subset_set(face, base_codepoints, BASE_COUNT);
  create_subset_set(face, subset_codepoints, SUBSET_COUNT);
  vector<uint8_t> precompressed = precompress_immutable(face);

  printf("mode, quality, duration_ms, iterations, patch_size, ms/req\n");
  unsigned start_mode = BASE_COUNT > 0 ? IMMUTABLE_LAYOUT : PRECOMPRESS_LAYOUT;
  for (unsigned mode = start_mode; mode < CUSTOM_DIFF; mode++) {
    for (unsigned quality = 0; quality <= 9; quality++) {
      unsigned patch_size = 0;
      unsigned duration_ms = 0;
      auto start = high_resolution_clock::now();

      unsigned i = 0;
      while (true) {
        Operation op(hb_face_reference_blob(face),
                     hb_set_reference(base_codepoints),
                     hb_set_reference(subset_codepoints));
        op.dynamic_quality = quality;
        op.mode = (Mode)mode;
        op.precompressed = precompressed;

        op.MakeSubsets();
        patch_size = op.MakePatch(i);

        if (i % 20 == 0) {
          auto stop = high_resolution_clock::now();
          duration_ms = duration_cast<milliseconds>(stop - start).count();
          if (duration_ms > TRIAL_DURATION_MS) {
            break;
          }
        }
        i++;
      }

      float ms_per_request = (float)duration_ms / (float)i;
      printf("%s, %u, %u, %u, %u, %.2f\n", mode_to_string((Mode)mode), quality,
             duration_ms, i, patch_size, ms_per_request);
    }
  }

  hb_set_destroy(base_codepoints);
  hb_set_destroy(subset_codepoints);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "ERROR: invalid args." << std::endl;
    return -1;
  }

  char* font_path = argv[1];

  hb_blob_t* font_blob = hb_blob_create_from_file_or_fail(font_path);
  if (!font_blob) {
    std::cout << "ERROR: invalid file path." << std::endl;
    return -1;
  }

  hb_face_t* face = hb_face_create(font_blob, 0);

  // test_precompression(face);
  test_dictionary_size(face);

  hb_blob_destroy(font_blob);
  hb_face_destroy(face);
  return 0;
}
