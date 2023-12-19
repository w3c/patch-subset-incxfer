#include "util/helper.h"

#include "absl/strings/str_cat.h"
#include "common/axis_range.h"
#include "common/font_data.h"
#include "common/font_helper.h"

using absl::flat_hash_map;
using absl::StatusOr;
using absl::StrCat;
using common::AxisRange;
using common::FontData;
using common::FontHelper;

namespace util {

absl::StatusOr<absl::flat_hash_map<hb_tag_t, common::AxisRange>>
ParseDesignSpace(const std::vector<std::string>& list) {
  flat_hash_map<hb_tag_t, AxisRange> result;
  for (std::string item : list) {
    std::stringstream input(item);
    std::string tag_str;
    if (!getline(input, tag_str, '=')) {
      return absl::InvalidArgumentError(StrCat("Failed parsing (1) ", item));
    }

    std::string value1, value2;
    if (getline(input, value1, ':')) {
      getline(input, value2);
    }

    hb_tag_t tag = FontHelper::ToTag(tag_str);
    float start = std::stof(value1);

    if (value2.empty()) {
      result[tag] = AxisRange::Point(start);
      continue;
    }

    float end = std::stof(value2);
    auto r = AxisRange::Range(start, end);
    if (!r.ok()) {
      return r.status();
    }

    result[tag] = *r;
  }

  return result;
}

void check_ok(absl::Status status) {
  if (!status.ok()) {
    std::cerr << status << std::endl;
    exit(-1);
  }
}

StatusOr<FontData> load_data(const char* filename) {
  hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
  if (!blob) {
    return absl::InvalidArgumentError(
        StrCat("Failed to load file: ", filename));
  }

  FontData font;
  font.set(blob);
  hb_blob_destroy(blob);

  return font;
}

}  // namespace util