#include <filesystem>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/font_data.h"
#include "ift/ift_client.h"
#include "util/helper.h"

using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::FontData;
using ift::IFTClient;
using util::check_ok;
using util::load_data;

ABSL_FLAG(std::vector<std::string>, codepoints, {},
          "List of codepoints to add.");

ABSL_FLAG(std::vector<std::string>, design_space, {},
          "Design space to add."
          "Example: wght=300,wdth=50:100");

void load_patches(std::filesystem::path base_dir, IFTClient& client) {
  StatusOr<IFTClient::State> state;
  do {
    for (const auto& path : client.PatchesNeeded()) {
      std::filesystem::path p = base_dir / path;
      auto patch = load_data(p.c_str());
      check_ok(patch);
      client.AddPatch(path, *patch);
      std::cerr << "  applied " << p << std::endl;
    }

    std::cerr << "  ran client process." << std::endl;
    state = client.Process();
    check_ok(state);

  } while (*state != IFTClient::READY);
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() < 3) {
    std::cerr << "augment <input font> <patch dir>" << std::endl;
    return -1;
  }

  std::filesystem::path in_font_path = args[1];
  std::filesystem::path in_font_dir = args[2];
  auto in_font = load_data(in_font_path.c_str());
  check_ok(in_font);

  auto ift_client = IFTClient::NewClient(std::move(*in_font));
  check_ok(ift_client);

  for (std::string cp : absl::GetFlag(FLAGS_codepoints)) {
    uint32_t v = (uint32_t)std::stoul(cp, nullptr, 16);
    ift_client->AddDesiredCodepoints({v});
  }

  auto design_space = util::ParseDesignSpace(absl::GetFlag(FLAGS_design_space));
  check_ok(design_space);
  for (auto [tag, range] : *design_space) {
    check_ok(
        ift_client->AddDesiredDesignSpace(tag, range.start(), range.end()));
  }

  load_patches(in_font_dir, *ift_client);

  std::cout << ift_client->GetFontData().string();
  return 0;
}