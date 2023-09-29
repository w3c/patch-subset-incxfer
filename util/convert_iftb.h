#ifndef UTIL_CONVERT_IFTB_H_
#define UTIL_CONVERT_IFTB_H_

#include "absl/strings/string_view.h"
#include "hb.h"
#include "patch_subset/proto/IFT.pb.h"

namespace util {

IFT convert_iftb(absl::string_view iftb_dump, hb_face_t* face);

}

#endif  // UTIL_CONVERT_IFTB_H_
