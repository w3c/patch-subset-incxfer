#include "ift/proto/format_2_patch_map.h"

#include "gtest/gtest.h"
#include "ift/proto/patch_map.h"

namespace ift::proto {

class Format2PatchMapTest : public ::testing::Test {
 protected:
  Format2PatchMapTest() {}
};

constexpr int min_header_size = 34;
constexpr int min_entry_size = 1;
constexpr int min_codepoints_size = 4;

TEST_F(Format2PatchMapTest, RoundTrip) {
  PatchMap map;
  PatchMap::Coverage coverage{1, 2, 3, 4};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1);

  PatchMap map_out;
  std::string template_out;
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(template_out, uri_template);
  EXPECT_EQ(map, map_out);
}

// TODO:
// - test ext filtering

}  // namespace ift::proto