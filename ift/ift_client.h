#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace ift {

/*
 * Client library for IFT fonts. Provides common operations needed by a client
 * trying to use an IFT font.
 */
class IFTClient {
 public:
  static std::string PatchToUrl(absl::string_view url_template,
                                uint32_t patch_idx);
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_
