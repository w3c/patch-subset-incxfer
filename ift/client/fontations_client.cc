#include "ift/client/fontations_client.h"

#include "absl/status/status.h"
#include "common/font_data.h"
#include "ift/encoder/encoder.h"

using absl::Status;
using common::FontData;
using ift::encoder::Encoder;

namespace ift::client {

Status ToFile(const FontData& data, const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    return absl::InternalError("Unable to open file for output.");
  }

  fwrite(data.data(), 1, data.size(), f);
  fclose(f);
  return absl::OkStatus();
}

void ParseGraph(const std::string& text, graph& out) {
  std::stringstream ss(text);

  std::string line;
  while (getline(ss, line)) {
    std::stringstream line_ss(line);
    std::string node;
    if (!getline(line_ss, node, ';')) {
      continue;
    }

    auto& edges = out[node];

    std::string edge;
    while (getline(line_ss, edge, ';')) {
      edges.insert(edge);
    }
  }
}

absl::StatusOr<std::string> Exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  FILE* pipe = popen(cmd, "r");
  if (!pipe) {
    return absl::InternalError("Unable to start process.");
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    result += buffer.data();
  }
  if (pclose(pipe)) {
    return absl::InternalError("exec command failed.");
  }
  return result;
}

Status ToGraph(const Encoder& encoder, const FontData& base, graph& out) {
  char template_str[] = "encoder_test_XXXXXX";
  const char* temp_dir = mkdtemp(template_str);

  if (!temp_dir) {
    return absl::InternalError("Failed to create temp working directory.");
  }

  std::string font_path = absl::StrCat(temp_dir, "/font.ttf");
  auto sc = ToFile(base, font_path.c_str());
  if (!sc.ok()) {
    return sc;
  }

  for (auto& p : encoder.Patches()) {
    auto& path = p.first;
    auto& data = p.second;
    std::string full_path = absl::StrCat(temp_dir, "/", path);
    auto sc = ToFile(data, full_path.c_str());
    if (!sc.ok()) {
      return sc;
    }
  }

  std::string command =
      absl::StrCat("${TEST_SRCDIR}/fontations/ift_graph --font=", font_path);
  auto r = Exec(command.c_str());
  if (!r.ok()) {
    return r.status();
  }

  ParseGraph(*r, out);

  return absl::OkStatus();
}

}  // namespace ift::client