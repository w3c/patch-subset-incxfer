#ifndef IFT_URL_TEMPLATE_H_
#define IFT_URL_TEMPLATE_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace ift {

/*
 * Implementation of IFT URL template substitution.
 */
class URLTemplate {
 public:
  static std::string PatchToUrl(absl::string_view url_template,
                                uint32_t patch_idx);
};

}  // namespace ift

#endif  // IFT_URL_TEMPLATE_H_
