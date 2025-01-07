#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/axis_range.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/woff2.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/glyph_keyed_diff.h"
#include "ift/proto/patch_map.h"
#include "ift/url_template.h"
#include "util/helper.h"

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

ABSL_FLAG(uint32_t, input_iftb_patch_count, 0,
          "The number of input iftb patches there are. Should be set if using "
          "'iftb_patch_groups'.");

ABSL_FLAG(
    uint32_t, iftb_patch_groups, 0,
    "If using existing iftb patches this overrides input subsets and evenly "
    "divides the iftb patches into the specified number of groups for "
    "forming the non-outline data shared brotli patches.");

ABSL_FLAG(uint32_t, jump_ahead, 1, "Number of levels to encode at each node.");

ABSL_FLAG(std::vector<std::string>, optional_feature_tags, {},
          "A list of features to make optionally available via a patch.");

ABSL_FLAG(
    std::vector<std::string>, base_design_space, {},
    "Design space to cut the initial subset too. List of axis tag range pairs. "
    "Example: wght=300,wdth=50:100");

ABSL_FLAG(std::vector<std::string>, optional_design_space, {},
          "Design space to make available via an optional patch. "
          "List of axis tag range pairs. "
          "Example: wght=300,wdth=50:100");

ABSL_FLAG(std::string, optional_design_space_url_template, {},
          "Output URL template to be used for IFTB patches associated "
          "with the optional design space.");

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::AxisRange;
using common::FontData;
using common::FontHelper;
using common::Woff2;
using ift::GlyphKeyedDiff;
using ift::URLTemplate;
using ift::encoder::Encoder;
using ift::proto::PatchMap;
using util::ParseDesignSpace;

btree_set<hb_tag_t> StringsToTags(const std::vector<std::string>& tag_strs) {
  btree_set<hb_tag_t> tags;
  for (const auto& tag_str : tag_strs) {
    if (tag_str.size() != 4) {
      continue;
    }

    tags.insert(HB_TAG(tag_str[0], tag_str[1], tag_str[2], tag_str[3]));
  }
  return tags;
}

FontData load_iftb_patch(uint32_t index, const std::string& path_template) {
  std::string path = URLTemplate::PatchToUrl(path_template, index);
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(path.c_str());
  if (!blob) {
    fprintf(stderr, "failed to load file: %s\n", path.c_str());
    exit(-1);
  }

  FontData result(blob);
  return result;
}

FontData load_iftb_patch(uint32_t index) {
  std::string path_template = absl::GetFlag(FLAGS_input_iftb_patch_template);
  return load_iftb_patch(index, path_template);
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

void write_patch(const std::string& url, const FontData& patch) {
  std::string output_path = absl::GetFlag(FLAGS_output_path);
  std::cerr << "  Writing patch: " << StrCat(output_path, "/", url)
            << std::endl;
  auto sc = write_file(StrCat(output_path, "/", url), patch);
  if (!sc.ok()) {
    std::cerr << sc.message() << std::endl;
    exit(-1);
  }
}

void write_patch(const std::string& url_template, uint32_t id,
                 const FontData& patch) {
  std::string name = URLTemplate::PatchToUrl(url_template, id);
  write_patch(name, patch);
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
    write_patch(p.first, p.second);
  }

  return 0;
}

StatusOr<std::vector<btree_set<uint32_t>>> get_iftb_patch_groups_from_args(
    const std::vector<char*>& args) {
  std::vector<btree_set<uint32_t>> result;

  std::cout << ">> loading input iftb patches:" << std::endl;
  for (size_t i = 2; i < args.size(); i++) {
    auto s = load_unicodes_file(args[i]);
    btree_set<uint32_t> ordered;
    std::copy(s->begin(), s->end(), std::inserter(ordered, ordered.begin()));
    if (!s.ok()) {
      return s.status();
    }

    result.push_back(std::move(ordered));
  }

  return result;
}

std::vector<btree_set<uint32_t>> generate_iftb_patch_groups(
    uint32_t num_groups, uint32_t num_patches) {
  // don't include chunk 0 which is already in the base font.
  uint32_t per_group = (num_patches - 1) / num_groups;
  uint32_t remainder = (num_patches - 1) % num_groups;

  std::vector<btree_set<uint32_t>> result;
  uint32_t patch_idx = 1;
  for (uint32_t i = 0; i < num_groups; i++) {
    btree_set<uint32_t> group;
    uint32_t count = per_group;
    if (remainder > 0) {
      count++;
      remainder--;
    }
    for (uint32_t j = 0; j < count; j++) {
      group.insert(patch_idx++);
    }
    result.push_back(std::move(group));
  }

  return result;
}

Status configure_mixed_mode(std::vector<btree_set<uint32_t>> iftb_patch_groups,
                            Encoder& encoder) {
  for (const auto& grouping : iftb_patch_groups) {
    for (uint32_t id : grouping) {
      FontData iftb_patch = load_iftb_patch(id);
      auto sc = encoder.AddExistingIftbPatch(id, iftb_patch);
      if (!sc.ok()) {
        return sc;
      }
    }
  }

  flat_hash_map<hb_tag_t, AxisRange> design_space = {};
  if (!absl::GetFlag(FLAGS_base_design_space).empty()) {
    auto ds = ParseDesignSpace(absl::GetFlag(FLAGS_base_design_space));
    if (!ds.ok()) {
      return ds.status();
    }
    design_space = *ds;
  }

  Status sc = encoder.SetBaseSubsetFromIftbPatches({}, design_space);
  if (!sc.ok()) {
    return sc;
  }
  for (const auto& grouping : iftb_patch_groups) {
    flat_hash_set<uint32_t> set;
    std::copy(grouping.begin(), grouping.end(),
              std::inserter(set, set.begin()));
    sc = encoder.AddExtensionSubsetOfIftbPatches(set);
    if (!sc.ok()) {
      return sc;
    }
  }

  return absl::OkStatus();
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);
  // TODO(garretrieger): add support for taking arguments/config as a proto
  // file,
  //   where command line flags override the proto settings.
  bool mixed_mode = !absl::GetFlag(FLAGS_input_iftb_patch_template).empty();
  bool generated_groups = mixed_mode &&
                          absl::GetFlag(FLAGS_iftb_patch_groups) &&
                          absl::GetFlag(FLAGS_input_iftb_patch_count);

  if (args.size() < (generated_groups ? 2 : 3)) {
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

  Encoder encoder;
  encoder.SetUrlTemplate(absl::GetFlag(FLAGS_url_template));
  {
    hb_face_t* input_font = load_font(args[1]);
    encoder.SetFace(input_font);
    hb_face_destroy(input_font);
  }

  encoder.SetJumpAhead(absl::GetFlag(FLAGS_jump_ahead));
  auto feature_tags_str = absl::GetFlag(FLAGS_optional_feature_tags);
  encoder.AddOptionalFeatureGroup(StringsToTags(feature_tags_str));

  if (!absl::GetFlag(FLAGS_optional_design_space).empty()) {
    auto ds = ParseDesignSpace(absl::GetFlag(FLAGS_optional_design_space));
    if (!ds.ok()) {
      std::cerr << ds.status().message() << std::endl;
      return -1;
    }
    encoder.AddOptionalDesignSpace(*ds);
    std::string design_space_url_template =
        absl::GetFlag(FLAGS_optional_design_space_url_template);
    if (!design_space_url_template.empty()) {
      encoder.AddIftbUrlTemplateOverride(*ds, design_space_url_template);
    }
  }

  if (mixed_mode) {
    std::cout << ">> configuring encoder with iftb patches:" << std::endl;
    if (generated_groups) {
      auto sc =
          configure_mixed_mode(generate_iftb_patch_groups(
                                   absl::GetFlag(FLAGS_iftb_patch_groups),
                                   absl::GetFlag(FLAGS_input_iftb_patch_count)),
                               encoder);
      if (!sc.ok()) {
        std::cerr
            << "Failure configuring mixed mode with generated patch groups: "
            << sc.message() << std::endl;
        return -1;
      }
    } else {
      auto groups = get_iftb_patch_groups_from_args(args);
      if (!groups.ok()) {
        std::cerr << "Failure loading input patch groups: "
                  << groups.status().message() << std::endl;
        return -1;
      }
      auto sc = configure_mixed_mode(*groups, encoder);
      if (!sc.ok()) {
        std::cerr
            << "Failure configuring mixed mode with supplied patch groups: "
            << sc.message() << std::endl;
        return -1;
      }
    }
  } else {
    std::cout << ">> configuring encoder:" << std::endl;
    bool first = true;
    for (size_t i = 2; i < args.size(); i++) {
      auto s = load_unicodes_file(args[i]);
      if (!s.ok()) {
        std::cerr << s.status().message() << std::endl;
        return -1;
      }

      Status sc;
      if (first) {
        Encoder::SubsetDefinition base;
        base.codepoints = *s;
        if (!absl::GetFlag(FLAGS_base_design_space).empty()) {
          auto ds = ParseDesignSpace(absl::GetFlag(FLAGS_base_design_space));
          if (!ds.ok()) {
            std::cerr << ds.status().message() << std::endl;
            return -1;
          }
          base.design_space = *ds;
        }
        sc = encoder.SetBaseSubsetFromDef(base);
        if (!sc.ok()) {
          std::cerr << sc.message() << std::endl;
          return -1;
        }
        first = false;
      } else {
        encoder.AddExtensionSubset(*s);
      }
    }
  }

  std::cout << ">> encoding:" << std::endl;
  auto base_font = encoder.Encode();
  base_font = Woff2::EncodeWoff2(base_font->str(), false);
  if (!base_font.ok()) {
    std::cerr << base_font.status().message() << std::endl;
    return -1;
  }

  std::cout << ">> generating output patches:" << std::endl;
  return write_output(encoder, *base_font);
}
