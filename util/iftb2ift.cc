#include <google/protobuf/text_format.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/font_data.h"
#include "util/convert_iftb.h"

/*
 * This utility converts iftb fonts into the IFT format.
 */

// TODO(garretrieger): add flags to allow input info dump and an output
// file path to be specified. Currently these come from stdin/stdout.

// TODO(garretrieger): support converting IFTB feature mappings.

ABSL_FLAG(std::string, output_format, "font",
          "Format of the output: 'text', 'proto', 'font', or 'woff2'.");

using google::protobuf::TextFormat;
using ift::encoder::Encoder;
using ift::proto::IFT;
using ift::proto::IFTTable;
using patch_subset::FontData;
using util::convert_iftb;

hb_face_t* load_font(const char* filename) {
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", filename);
    exit(-1);
  }

  uint32_t length = 0;
  const char* data = hb_blob_get_data(blob, &length);

  // Input is an IFTB font which will have the first 4 bytes as
  // 'IFTB'. Our version of harfbuzz doesn't support this tag, so
  // rewrite 'IFTB' version tag to normal open type 0100
  char* copy = (char*)calloc(length, 1);
  memcpy(copy, data, length);
  copy[0] = 0;
  copy[1] = 1;
  copy[2] = 0;
  copy[3] = 0;

  hb_blob_destroy(blob);
  blob = hb_blob_create(copy, length, HB_MEMORY_MODE_READONLY, copy, free);

  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  return face;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() != 2) {
    std::cout
        << "This utility converts an existing IFTB font into the IFT format. "
        << "it must be supplied with a IFTB info dump on stdin "
        << "(produced via \"iftb -VV info\"). The output file is sent to "
           "stdout."
        << std::endl
        << std::endl;
    std::cout << "usage: iftb2ift [--output_format=<text|proto|font|woff2>] "
                 "<input font>\n";
    return -1;
  }

  hb_face_t* face = load_font(args[1]);

  std::string input;
  std::getline(std::cin, input, '\0');
  IFT ift = convert_iftb(input, face);

  std::string out_format = absl::GetFlag(FLAGS_output_format);
  if (out_format == "text") {
    std::string out;
    TextFormat::PrintToString(ift, &out);
    std::cout << out << std::endl;
  } else if (out_format == "proto") {
    std::cout << ift.SerializeAsString();
  } else if (out_format == "font" || out_format == "woff2") {
    auto out_font = IFTTable::AddToFont(face, ift, true);
    if (!out_font.ok()) {
      std::cerr << out_font.status();
      return -1;
    }

    if (out_format == "woff2") {
      out_font = Encoder::EncodeWoff2(out_font->str());
      if (!out_font.ok()) {
        std::cerr << out_font.status();
      }
    }

    std::cout << out_font->string();
  } else {
    fprintf(stderr, "ERROR: unrecognized output format: %s\n",
            absl::GetFlag(FLAGS_output_format).c_str());
    hb_face_destroy(face);
    return -1;
  }

  hb_face_destroy(face);
  return 0;
}
