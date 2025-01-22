#include "util/convert_iftb.h"

#include <google/protobuf/text_format.h>

#include <string>

#include "absl/container/flat_hash_set.h"
#include "common/font_data.h"
#include "common/sparse_bit_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::Eq;

using absl::flat_hash_set;
using common::FontData;
using common::hb_blob_unique_ptr;
using common::make_hb_blob;
using ::common::SparseBitSet;
using google::protobuf::TextFormat;

namespace util {

class ConvertIftbTest : public ::testing::Test {
 protected:
  ConvertIftbTest() {
    hb_blob_unique_ptr blob =
        common::make_hb_blob(hb_blob_create_from_file_or_fail(
            "util/testdata/convert-iftb-sample.txt"));
    assert(blob.get());

    FontData blob_data(blob.get());
    sample_input = blob_data.string();
  }

  std::string sample_input;
};

TEST_F(ConvertIftbTest, BasicConversion) {
  auto config = convert_iftb(sample_input);
  ASSERT_TRUE(config.ok()) << config.status();

  std::string expected_config =
      "glyph_segments {\n"
      "  key: 0\n"
      "  value {\n"
      "    values: 0\n"
      "    values: 5\n"
      "  }\n"
      "}\n"
      "glyph_segments {\n"
      "  key: 1\n"
      "  value {\n"
      "    values: 1\n"
      "    values: 2\n"
      "    values: 3\n"
      "  }\n"
      "}\n"
      "glyph_segments {\n"
      "  key: 2\n"
      "  value {\n"
      "    values: 4\n"
      "    values: 6\n"
      "  }\n"
      "}\n"
      "initial_glyph_patches {\n"
      "  values: 0\n"
      "}\n"
      "glyph_patch_groupings {\n"
      "  values: 1\n"
      "  values: 2\n"
      "}\n";
  std::string config_string;
  TextFormat::PrintToString(*config, &config_string);

  ASSERT_EQ(config_string, expected_config);
}

}  // namespace util
