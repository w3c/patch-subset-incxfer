#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/proto/IFT.pb.h"
#include "util/convert_iftb.h"

ABSL_FLAG(std::string, output_format, "font",
          "Format of the output: 'text', 'proto', or 'font'.");

using google::protobuf::TextFormat;
using patch_subset::FontData;
using util::convert_iftb;

hb_face_t* load_font(const char* filename) {
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", filename);
    exit(-1);
  }

  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  return face;
}

FontData replace_iftb_table(hb_face_t* face, const FontData& ift) {
  constexpr uint32_t max_tags = 64;
  hb_tag_t table_tags[max_tags + 1];
  unsigned table_count = max_tags;
  unsigned tot_table_count =
      hb_face_get_table_tags(face, 0, &table_count, table_tags);
  if (tot_table_count != table_count) {
    fprintf(stderr, "ERROR: more than 64 tables present in input font.");
    exit(-1);
  }

  table_tags[table_count + 1] = 0;
  hb_face_t* new_face = hb_face_builder_create();

  for (unsigned i = 0; i < table_count; i++) {
    hb_tag_t tag = table_tags[i];
    hb_blob_t* blob = hb_face_reference_table(face, tag);

    if (tag == HB_TAG('I', 'F', 'T', 'B')) {
      tag = HB_TAG('I', 'F', 'T', ' ');
      table_tags[i] = tag;

      hb_blob_destroy(blob);
      blob = ift.reference_blob();
    }

    hb_face_builder_add_table(new_face, tag, blob);
    hb_blob_destroy(blob);
  }

  // keep original sort order.
  hb_face_builder_sort_tables(new_face, table_tags);

  hb_blob_t* blob = hb_face_reference_blob(new_face);
  hb_face_destroy(new_face);

  FontData updated(blob);
  hb_blob_destroy(blob);

  return updated;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() != 2) {
    printf("usage: [--notext_format] <input font>\n");
    return -1;
  }

  hb_face_t* face = load_font(args[1]);

  std::string input;
  std::getline(std::cin, input, '\0');
  IFT ift = convert_iftb(input, face);

  if (absl::GetFlag(FLAGS_output_format) == "text") {
    std::string out;
    TextFormat::PrintToString(ift, &out);
    std::cout << out << std::endl;
  } else if (absl::GetFlag(FLAGS_output_format) == "proto") {
    std::cout << ift.SerializeAsString();
  } else if (absl::GetFlag(FLAGS_output_format) == "font") {
    FontData ift_bin(ift.SerializeAsString());
    FontData out_font = replace_iftb_table(face, ift_bin);
    std::cout << out_font.string();
  } else {
    fprintf(stderr, "ERROR: unrecognized output format: %s\n",
            absl::GetFlag(FLAGS_output_format).c_str());
    hb_face_destroy(face);
    return -1;
  }

  hb_face_destroy(face);
  return 0;
}
