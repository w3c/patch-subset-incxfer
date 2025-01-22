/*
 * This utility converts an iftb info dump into the corresponding
 * encoding_config.proto config file.
 *
 * Takes the info dump on stdin and outputs the config on stdout.
 */

#include <google/protobuf/text_format.h>

#include <iostream>
#include <sstream>

#include "absl/flags/parse.h"
#include "util/convert_iftb.h"

int main(int argc, char** argv) {
  auto args = absl::ParseCommandLine(argc, argv);

  std::stringstream ss;
  ss << std::cin.rdbuf();
  std::string input = ss.str();

  auto config = util::convert_iftb(input);
  if (!config.ok()) {
    std::cerr << "Failure parsing iftb info dump: " << config.status()
              << std::endl;
    return -1;
  }

  std::string out;
  google::protobuf::TextFormat::PrintToString(*config, &out);

  std::cout << out << std::endl;
  return 0;
}