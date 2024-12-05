#include "ift/ift_client.h"

#include <cstdint>

#include "base32_hex.hpp"
#include "uritemplate.hpp"

using cppcodec::base32_hex;
using uritemplatecpp::UriTemplate;

namespace ift {

std::string IFTClient::PatchToUrl(absl::string_view url_template,
                                  uint32_t patch_idx) {
  uint8_t bytes[4];
  bytes[0] = (patch_idx >> 24) & 0x000000FFu;
  bytes[1] = (patch_idx >> 16) & 0x000000FFu;
  bytes[2] = (patch_idx >> 8) & 0x000000FFu;
  bytes[3] = patch_idx & 0x000000FFu;

  size_t start = 0;
  while (start < 3 && !bytes[start]) {
    start++;
  }

  std::string result = base32_hex::encode(bytes + start, 4 - start);
  result.erase(std::find_if(result.rbegin(), result.rend(),
                            [](unsigned char ch) { return ch != '='; })
                   .base(),
               result.end());

  std::string url_template_copy{url_template};
  UriTemplate uri(url_template_copy);
  uri.set("id", result);

  if (result.size() >= 1) {
    uri.set("d1", result.substr(result.size() - 1, 1));
  } else {
    uri.set("d1", "_");
  }

  if (result.size() >= 2) {
    uri.set("d2", result.substr(result.size() - 2, 1));
  } else {
    uri.set("d2", "_");
  }

  if (result.size() >= 3) {
    uri.set("d3", result.substr(result.size() - 3, 1));
  } else {
    uri.set("d3", "_");
  }

  if (result.size() >= 4) {
    uri.set("d4", result.substr(result.size() - 4, 1));
  } else {
    uri.set("d4", "_");
  }

  // TODO(garretrieger): add additional variable id64

  return uri.build();
}

}  // namespace ift
