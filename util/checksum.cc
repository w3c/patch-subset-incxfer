#include <iostream>
#include <string>

#include "common/status.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/font_data.h"

using ::patch_subset::FastHasher;
using ::patch_subset::FileFontProvider;
using ::patch_subset::FontData;
using ::patch_subset::StatusCode;

int main(int argc, char** argv) {
  FileFontProvider font_provider("");
  if (argc != 2) {
    std::cout << "Usage: fingerprint <file>" << std::endl;
  }

  std::string file_path(argv[1]);
  FontData file_data;
  if (font_provider.GetFont(file_path, &file_data) != StatusCode::kOk) {
    std::cout << "File not found: " << file_path << std::endl;
    return -1;
  }

  FastHasher hasher;
  uint64_t checksum = hasher.Checksum(file_data.str());

  std::cout << "Checksum = 0x" << std::uppercase << std::hex << checksum
            << std::endl;
}
