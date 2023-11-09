#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/ift_client.h"
#include "patch_subset/font_data.h"

/*
 * Utility that converts a standard font file into an IFT font file.
 */

ABSL_FLAG(std::string, output_path, "./",
          "Path to write output files under (base font and patches).");

ABSL_FLAG(std::string, output_font, "out.ttf",
          "Name of the outputted base font.");

ABSL_FLAG(std::string, url_template, "patch$5$4$3$2$1.br",
          "Url template for patch files.");

ABSL_FLAG(std::string, input_iftb_patch_template, "",
          "Template used to locate existing iftb patches which should be "
          "used in the output IFT font. If set the input codepoints files "
          "are interpretted as patch indices instead of codepoints.");

using absl::btree_set;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using ift::IFTClient;
using ift::encoder::Encoder;
using patch_subset::FontData;

FontData load_iftb_patch(uint32_t index) {
  std::string path = IFTClient::PatchToUrl(
      absl::GetFlag(FLAGS_input_iftb_patch_template), index);
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(path.c_str());
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", path.c_str());
    exit(-1);
  }

  FontData result(blob);
  return result;
}

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

void write_patch(const Encoder& encoder, uint32_t id, const FontData& patch) {
  std::string output_path = absl::GetFlag(FLAGS_output_path);

  std::string name = IFTClient::PatchToUrl(encoder.UrlTemplate(), id);
  std::cerr << "  Writing patch: " << StrCat(output_path, "/", name)
            << std::endl;
  auto sc = write_file(StrCat(output_path, "/", name), patch);
  if (!sc.ok()) {
    std::cerr << sc.message() << std::endl;
    exit(-1);
  }
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
    write_patch(encoder, p.first, p.second);
  }

  return 0;
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);
  // TODO(garretrieger): add support for taking arguments/config as a proto
  // file,
  //   where command line flags override the proto settings.
  if (args.size() < 3) {
    std::cerr
        << "creates an IFT font from <input font> that can incrementally load "
        << "the provided subsets." << std::endl
        << std::endl
        << "usage: <input font> <codepoints file> [<codepoints file> ...]"
        << std::endl
        << "flags: " << std::endl
        << " --output_path: directory to write the output font and patches "
           "into."
        << std::endl
        << " --output_font: name of the output font file." << std::endl
        << " --url_template: url template to use for the patch files."
        << std::endl
        << std::endl;
    return -1;
  }

  bool mixed_mode = !absl::GetFlag(FLAGS_input_iftb_patch_template).empty();

  Encoder encoder;
  encoder.SetUrlTemplate(absl::GetFlag(FLAGS_url_template));
  {
    hb_face_t* input_font = load_font(args[1]);
    encoder.SetFace(input_font);
    hb_face_destroy(input_font);
  }

  bool first = true;
  if (mixed_mode) {
    // Load and write out iftb patches
    std::cout << ">> loading input iftb patches:" << std::endl;
    for (size_t i = 2; i < args.size(); i++) {
      auto s = load_unicodes_file(args[i]);
      btree_set<uint32_t> ordered;
      std::copy(s->begin(), s->end(), std::inserter(ordered, ordered.begin()));
      if (!s.ok()) {
        std::cerr << "Failed to load codepoint file (" << args[i]
                  << "): " << s.status().message() << std::endl;
        return -1;
      }

      for (uint32_t id : ordered) {
        FontData iftb_patch = load_iftb_patch(id);
        auto sc = encoder.AddExistingIftbPatch(id, iftb_patch);
        if (!sc.ok()) {
          std::cout << "Failed adding existing iftb patch: " << sc.message()
                    << std::endl;
          return -1;
        }

        if (!first) {
          write_patch(encoder, id, iftb_patch);
        }
      }

      first = false;
    }
  }

  std::cout << ">> configuring encoder:" << std::endl;
  first = true;
  for (size_t i = 2; i < args.size(); i++) {
    auto s = load_unicodes_file(args[i]);
    if (!s.ok()) {
      std::cerr << s.status().message() << std::endl;
      return -1;
    }

    Status sc;
    if (first) {
      sc = !mixed_mode ? encoder.SetBaseSubset(*s)
                       : encoder.SetBaseSubsetFromIftbPatches(*s);
      if (!sc.ok()) {
        std::cerr << sc.message() << std::endl;
        return -1;
      }
      first = false;
    } else if (!mixed_mode) {
      encoder.AddExtensionSubset(*s);
    } else {
      sc = encoder.AddExtensionSubsetOfIftbPatches(*s);
      if (!sc.ok()) {
        std::cerr << sc.message() << std::endl;
        return -1;
      }
    }
  }

  std::cout << ">> encoding:" << std::endl;
  auto base_font = encoder.Encode();
  base_font = Encoder::EncodeWoff2(base_font->str(), false);
  if (!base_font.ok()) {
    std::cerr << base_font.status().message() << std::endl;
    return -1;
  }

  std::cout << ">> generating output patches:" << std::endl;
  return write_output(encoder, *base_font);
}