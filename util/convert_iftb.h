#ifndef UTIL_CONVERT_IFTB_H_
#define UTIL_CONVERT_IFTB_H_

#include "absl/strings/string_view.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"

namespace util {

ift::proto::IFT convert_iftb(absl::string_view iftb_dump, hb_face_t* face);

}

#endif  // UTIL_CONVERT_IFTB_H_
