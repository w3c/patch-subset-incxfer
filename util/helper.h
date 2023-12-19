#ifndef UTIL_HELPER_H_
#define UTIL_HELPER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "common/axis_range.h"
#include "common/font_data.h"
#include "hb.h"

namespace util {

absl::StatusOr<absl::flat_hash_map<hb_tag_t, common::AxisRange>>
ParseDesignSpace(const std::vector<std::string>& list);

void check_ok(absl::Status status);

template <typename S>
void check_ok(const S& status) {
  if (!status.ok()) {
    std::cerr << status.status() << std::endl;
    exit(-1);
  }
}

absl::StatusOr<common::FontData> load_data(const char* filename);

}  // namespace util

#endif  // UTIL_HELPER_H_
