#include <filesystem>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/binary_patch.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"
#include "ift/ift_client.h"
#include "ift/iftb_binary_patch.h"
#include "ift/per_table_brotli_binary_patch.h"
#include "util/helper.h"

using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::BinaryPatch;
using common::BrotliBinaryPatch;
using common::FontData;
using ift::IftbBinaryPatch;
using ift::IFTClient;
using ift::PerTableBrotliBinaryPatch;
using util::check_ok;
using util::load_data;

ABSL_FLAG(std::string, patch_format, "iftb",
          "Format of the patch. Can be 'iftb', 'sbr', or 'ptsbr'.");

const BinaryPatch* get_patcher() {
  if (absl::GetFlag(FLAGS_patch_format) == "iftb") {
    static IftbBinaryPatch iftb;
    return &iftb;
  }

  if (absl::GetFlag(FLAGS_patch_format) == "sbr") {
    static BrotliBinaryPatch brotli;
    return &brotli;
  }

  if (absl::GetFlag(FLAGS_patch_format) == "ptsbr") {
    static PerTableBrotliBinaryPatch pt_brotli;
    return &pt_brotli;
  }

  std::cerr << "Unrecognized patch format: "
            << absl::GetFlag(FLAGS_patch_format) << std::endl;
  exit(-1);
}

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  if (args.size() < 3) {
    std::cerr << "augment <input font> <input patch>" << std::endl;
    return -1;
  }

  auto in_font = load_data(args[1]);
  auto in_patch = load_data(args[2]);
  check_ok(in_font);
  check_ok(in_patch);

  const BinaryPatch* patcher = get_patcher();

  FontData result;
  auto sc = patcher->Patch(*in_font, *in_patch, &result);
  check_ok(sc);

  std::cout << result.string();

  return 0;
}