#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/ift_client.h"
#include "patch_subset/font_data.h"

ABSL_FLAG(std::string, output_path, "./",
          "Path to write output files under (base font and patches).");

ABSL_FLAG(std::string, output_font, "out.ttf",
          "Name of the outputted base font.");

ABSL_FLAG(std::string, url_template, "patch$5$4$3$2$1.br",
          "Url template for patch files.");

using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using ift::IFTClient;
using ift::encoder::Encoder;
using patch_subset::FontData;

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

StatusOr<flat_hash_set<hb_codepoint_t>> load_unicodes_file(
    const char* filename) {
  std::ifstream input;
  input.open(filename, std::ios::in);
  if (!input.is_open()) {
    return absl::NotFoundError(StrCat(filename, " was not found."));
  }
  flat_hash_set<hb_codepoint_t> result;

  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::stringstream ss(line);
    hb_codepoint_t cp;
    ss >> std::hex >> cp;
    result.insert(cp);
  }

  input.close();

  return result;
}

Status write_file(const std::string& name, const FontData& data) {
  std::ofstream output(name,
                       std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    return absl::NotFoundError(StrCat("File ", name, " was not found."));
  }
  output.write(data.data(), data.size());
  if (output.bad()) {
    output.close();
    return absl::InternalError(StrCat("Failed to write to ", name, "."));
  }

  output.close();
  return absl::OkStatus();
}

int write_output(const Encoder& encoder, const FontData& base_font) {
  std::string output_path = absl::GetFlag(FLAGS_output_path);
  std::string output_font = absl::GetFlag(FLAGS_output_font);

  std::cerr << "Writing base font: " << StrCat(output_path, "/", output_font)
            << std::endl;
  auto sc = write_file(StrCat(output_path, "/", output_font), base_font);
  if (!sc.ok()) {
    std::cerr << sc.message() << std::endl;
    return -1;
  }

  for (const auto& p : encoder.Patches()) {
    std::string name = IFTClient::PatchToUrl(encoder.UrlTemplate(), p.first);

    std::cerr << "Writing patch: " << StrCat(output_path, "/", name)
              << std::endl;
    auto sc = write_file(StrCat(output_path, "/", name), p.second);
    if (!sc.ok()) {
      std::cerr << sc.message() << std::endl;
      return -1;
    }
  }

  return 0;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);
  // TODO(garretrieger): add support for taking arguments/config as a proto
  // file,
  //   where command line flags override the proto settings.
  if (args.size() < 3) {
    std::cerr << "usage: <input font> <codepoints file> [<codepoints file> ...]"
              << std::endl;
    std::cerr
        << "creates an IFT font from <input font> that can incrementally load "
        << "the provided subsets." << std::endl;
  }

  hb_face_t* input_font = load_font(args[1]);

  std::vector<flat_hash_set<hb_codepoint_t>> subsets;
  for (size_t i = 2; i < args.size(); i++) {
    auto s = load_unicodes_file(args[i]);
    if (!s.ok()) {
      std::cerr << s.status().message() << std::endl;
      return -1;
    }
    subsets.push_back(std::move(*s));
  }

  std::vector<const flat_hash_set<uint32_t>*> subset_pointers;
  for (size_t i = 1; i < subsets.size(); i++) {
    subset_pointers.push_back(&(subsets[i]));
  }

  Encoder encoder;
  encoder.SetUrlTemplate(absl::GetFlag(FLAGS_url_template));
  auto base_font = encoder.Encode(input_font, subsets[0], subset_pointers);
  if (!base_font.ok()) {
    std::cerr << base_font.status().message() << std::endl;
    return -1;
  }

  return write_output(encoder, *base_font);
}