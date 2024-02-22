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

TEST_F(Format2PatchMapTest, RoundTrip_Simple) {
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

TEST_F(Format2PatchMapTest, RoundTrip_ComplexSet) {
  PatchMap map;
  PatchMap::Coverage coverage{123, 155, 179, 180, 181, 182, 1013};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_GT(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1);

  PatchMap map_out;
  std::string template_out;
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(template_out, uri_template);
  EXPECT_EQ(map, map_out);
}

TEST_F(Format2PatchMapTest, RoundTrip_Features) {
  PatchMap map;
  PatchMap::Coverage coverage{1, 2, 3, 4};
  coverage.features.insert(HB_TAG('w', 'g', 'h', 't'));
  coverage.features.insert(HB_TAG('w', 'd', 't', 'h'));
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1 +
                                   9 /* features */);

  PatchMap map_out;
  std::string template_out;
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(template_out, uri_template);
  EXPECT_EQ(map, map_out);
}

TEST_F(Format2PatchMapTest, RoundTrip_NonDefaultPatchEncoding) {
  PatchMap map;
  PatchMap::Coverage coverage1{1, 2, 3, 4};
  map.AddEntry(coverage1, 1, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage2{15, 16, 17, 18};
  map.AddEntry(coverage2, 2, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage3{25, 26, 27, 28};
  map.AddEntry(coverage3, 3, IFTB_ENCODING);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + uri_template.length() +
                3 * (min_entry_size + min_codepoints_size + 1) +
                1 /* non default encoding */);

  PatchMap map_out;
  std::string template_out;
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(template_out, uri_template);
  EXPECT_EQ(map, map_out);
}

TEST_F(Format2PatchMapTest, RoundTrip_IndexDeltas) {
  PatchMap map;
  PatchMap::Coverage coverage1{1, 2, 3, 4};
  map.AddEntry(coverage1, 7, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage2{15, 16, 17, 18};
  map.AddEntry(coverage2, 4, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage3{25, 26, 27, 28};
  map.AddEntry(coverage3, 10, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + uri_template.length() +
                3 * (min_entry_size + min_codepoints_size + 1) +
                3 * 2 /* index deltas */);

  PatchMap map_out;
  std::string template_out;
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(template_out, uri_template);
  EXPECT_EQ(map, map_out);
}

TEST_F(Format2PatchMapTest, RoundTrip_FilterExtensionEntries) {
  PatchMap map;
  PatchMap::Entry entry1({1, 2, 3, 4}, 1, SHARED_BROTLI_ENCODING, true);
  map.AddEntry(entry1.coverage, entry1.patch_index, entry1.encoding,
               entry1.extension_entry);

  PatchMap::Entry entry2({5, 6, 7, 8}, 2, SHARED_BROTLI_ENCODING, false);
  map.AddEntry(entry2.coverage, entry2.patch_index, entry2.encoding,
               entry2.extension_entry);

  PatchMap::Entry entry3({9, 10, 11, 12}, 3, SHARED_BROTLI_ENCODING, true);
  map.AddEntry(entry3.coverage, entry3.patch_index, entry3.encoding,
               entry3.extension_entry);

  std::string uri_template = "foo/$1";
  auto encoded = Format2PatchMap::Serialize(map, false, uri_template);
  auto encoded_extensions = Format2PatchMap::Serialize(map, true, uri_template);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  ASSERT_TRUE(encoded_extensions.ok()) << encoded_extensions.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1 +
                                   2 /* index delta */);
  ASSERT_EQ(encoded_extensions->length(),
            min_header_size + uri_template.length() +
                2 * (min_entry_size + min_codepoints_size + 1) +
                2 /* index delta */);

  PatchMap map_out;
  std::string template_out;

  // No extensions
  auto s = Format2PatchMap::Deserialize(*encoded, map_out, template_out);
  ASSERT_TRUE(s.ok()) << s;
  EXPECT_EQ(template_out, uri_template);
  std::vector<PatchMap::Entry> expected1{entry2};
  EXPECT_EQ(map_out.GetEntries(), expected1);

  // With Extensions
  map_out = {};
  s = Format2PatchMap::Deserialize(*encoded_extensions, map_out, template_out,
                                   true);
  ASSERT_TRUE(s.ok()) << s;
  EXPECT_EQ(template_out, uri_template);
  std::vector<PatchMap::Entry> expected2{entry1, entry3};
  EXPECT_EQ(map_out.GetEntries(), expected2);
}

}  // namespace ift::proto