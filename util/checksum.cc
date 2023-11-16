#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "common/file_font_provider.h"
#include "common/font_data.h"
#include "patch_subset/fast_hasher.h"

using absl::Status;
using common::FileFontProvider;
using common::FontData;
using patch_subset::FastHasher;

int main(int argc, char** argv) {
  FileFontProvider font_provider("");
  if (argc != 2) {
    std::cout << "Usage: checksum <file>" << std::endl;
  }

  std::string file_path(argv[1]);
  FontData file_data;
  Status s;
  if (!(s = font_provider.GetFont(file_path, &file_data)).ok()) {
    std::cout << "File not found: " << file_path << " (" << s << ")"
              << std::endl;
    return -1;
  }

  FastHasher hasher;
  uint64_t checksum = hasher.Checksum(file_data.str());

  std::cout << "Checksum = 0x" << std::uppercase << std::hex << checksum
            << std::endl;
}
