#include <hb.h>

#include <iostream>
#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "base64.h"
#include "patch_subset/cbor/compressed_set.h"
#include "patch_subset/cbor/patch_request.h"
#include "patch_subset/compressed_set.h"

using absl::Status;
using patch_subset::cbor::CompressedSet;
using patch_subset::cbor::PatchRequest;

void print_compressed_set(const CompressedSet& value) {
  hb_set_t* set = hb_set_create();

  if (!::patch_subset::CompressedSet::Decode(value, set).ok()) {
    std::cout << "  ERR_DECODE" << std::endl;
  }

  std::cout << "{";
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(set, &cp)) {
    std::cout << std::hex << cp << ", ";
  }
  std::cout << "}\n";

  hb_set_destroy(set);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: print-request <base64 encoded request>" << std::endl;
  }

  std::string encoded_request(argv[1]);
  std::string raw = base64_decode(encoded_request);

  PatchRequest request;
  auto status = PatchRequest::ParseFromString(raw, request);
  if (!status.ok()) {
    std::cout << "failed to decode." << std::endl;
    return -1;
  }

  if (request.HasCodepointsHave()) {
    std::cout << "codepoints_have = ";
    print_compressed_set(request.CodepointsHave());
  }

  if (request.HasCodepointsNeeded()) {
    std::cout << "codepoints_needed = ";
    print_compressed_set(request.CodepointsNeeded());
  }

  if (request.HasIndicesHave()) {
    std::cout << "indices_have = ";
    print_compressed_set(request.IndicesHave());
  }

  if (request.HasIndicesNeeded()) {
    std::cout << "indices_needed = ";
    print_compressed_set(request.IndicesNeeded());
  }

  if (request.HasOrderingChecksum()) {
    std::cout << "ordering_checksum = ";
    std::cout << std::hex << request.OrderingChecksum() << std::endl;
  }

  if (request.HasOriginalFontChecksum()) {
    std::cout << "original_font_checksum = ";
    std::cout << std::hex << request.OriginalFontChecksum() << std::endl;
  }

  if (request.HasBaseChecksum()) {
    std::cout << "base_checksum = ";
    std::cout << std::hex << request.BaseChecksum() << std::endl;
  }

  return 0;
}