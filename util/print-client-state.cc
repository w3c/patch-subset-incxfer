#include <hb.h>

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "common/font_data.h"
#include "patch_subset/cbor/client_state.h"

using absl::Status;
using common::FontData;
using patch_subset::cbor::ClientState;

void print_ordering(const std::vector<int32_t>& ordering) {
  std::cout << "[" << std::endl;
  for (size_t i = 0; i < ordering.size(); i++) {
    std::cout << "  " << ordering[i];
    if (i + 1 < ordering.size()) {
      std::cout << ",";
    }
    std::cout << std::endl;
  }
  std::cout << "]";
}

char* read_file(const std::string& path, size_t& size_out) {
  std::ifstream input_file;
  input_file.open(path, std::ios::binary);
  if (!input_file.is_open()) {
    std::cout << "file not found: " << path << std::endl;
    return nullptr;
  }

  input_file.seekg(0, std::ios::end);
  std::streampos size = input_file.tellg();
  input_file.seekg(0, std::ios::beg);
  char* buffer = new char[size];

  input_file.read(buffer, size);
  input_file.close();

  size_out = size;
  return buffer;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: print-client-state <path to ift font file>"
              << std::endl;
  }

  std::string input_file_path(argv[1]);
  size_t size;
  char* buffer = read_file(input_file_path, size);
  if (!buffer) {
    return -1;
  }

  common::FontData font(std::string_view(buffer, size));
  hb_face_t* face = font.reference_face();
  auto r = ClientState::FromFont(face);
  hb_face_destroy(face);

  if (!r.ok()) {
    std::cout << "Failed to load client state from IFTP table." << std::endl;
    return -1;
  }

  const ClientState& state = *r;
  if (state.HasOriginalFontChecksum()) {
    std::cout << "original_font_checksum = " << std::hex
              << state.OriginalFontChecksum() << std::endl;
  }

  if (state.HasCodepointOrdering()) {
    std::cout << "codepoint_ordering = ";
    print_ordering(state.CodepointOrdering());
    std::cout << std::endl;
  }

  return 0;
}
