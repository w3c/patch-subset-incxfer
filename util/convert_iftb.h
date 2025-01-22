#ifndef UTIL_CONVERT_IFTB_H_
#define UTIL_CONVERT_IFTB_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "util/encoder_config.pb.h"

namespace util {

absl::StatusOr<EncoderConfig> convert_iftb(absl::string_view iftb_dump);

}

#endif  // UTIL_CONVERT_IFTB_H_
