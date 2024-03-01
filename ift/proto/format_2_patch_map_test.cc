#include "ift/proto/format_2_patch_map.h"

#include "common/axis_range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

using testing::UnorderedElementsAre;

namespace ift::proto {

class Format2PatchMapTest : public ::testing::Test {
 protected:
  Format2PatchMapTest() {}
};

constexpr int min_header_size = 34;
constexpr int min_entry_size = 1;
constexpr int min_codepoints_size = 1;
constexpr int min_feature_design_space_size = 3;
constexpr int segment_size = 12;

TEST_F(Format2PatchMapTest, RoundTrip_Simple) {
  IFTTable table;
  uint32_t id[4]{1, 2, 3, 4};
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{1, 2, 3};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  table.SetUrlTemplate("foo/$1");
  auto sc = table.SetId(id);
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size +
                                   table.GetUrlTemplate().length() +
                                   min_entry_size + min_codepoints_size + 1);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, InvalidEntry) {
  IFTTable table;
  uint32_t id[4]{1, 2, 3, 4};
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{1, 2, 3};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  table.SetUrlTemplate("foo/$1");
  auto sc = table.SetId(id);
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size +
                                   table.GetUrlTemplate().length() +
                                   min_entry_size + min_codepoints_size + 1);

  std::string corrupt = encoded->substr(0, encoded->length() - 1);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(corrupt, out, false);
  ASSERT_TRUE(absl::IsInvalidArgument(s)) << s;
}

TEST_F(Format2PatchMapTest, RoundTrip_TwoByteBias) {
  IFTTable table;
  uint32_t id[4]{1, 2, 3, 4};
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{10251, 10252, 10253};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  table.SetUrlTemplate("foo/$1");
  auto sc = table.SetId(id);
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + table.GetUrlTemplate().length() + min_entry_size +
                min_codepoints_size + 1 + +2 /* bias */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_ThreeByteBias) {
  IFTTable table;
  uint32_t id[4]{1, 2, 3, 4};
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{100251, 100252, 100253};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  table.SetUrlTemplate("foo/$1");
  auto sc = table.SetId(id);
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + table.GetUrlTemplate().length() + min_entry_size +
                min_codepoints_size + 1 + +3 /* bias */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_ComplexSet) {
  IFTTable table;
  uint32_t id[4]{1, 2, 3, 4};
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{123, 155, 179, 180, 181, 182, 1013};
  map.AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  auto sc = table.SetId(id);
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_GT(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_Features) {
  IFTTable table;

  PatchMap::Coverage coverage{1, 2, 3};
  coverage.features.insert(HB_TAG('w', 'g', 'h', 't'));
  coverage.features.insert(HB_TAG('w', 'd', 't', 'h'));
  table.GetPatchMap().AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1 +
                                   11 /* features + segments */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_DesignSpace) {
  IFTTable table;

  PatchMap::Coverage coverage{1, 2, 3};
  coverage.design_space[HB_TAG('w', 'g', 'h', 't')] =
      *common::AxisRange::Range(100.0f, 200.0f);
  coverage.design_space[HB_TAG('w', 'd', 't', 'h')] =
      common::AxisRange::Point(0.75f);
  table.GetPatchMap().AddEntry(coverage, 1, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1 +
                                   min_feature_design_space_size +
                                   segment_size * 2);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_NonDefaultPatchEncoding) {
  IFTTable table;

  PatchMap::Coverage coverage1{1, 2, 3};
  table.GetPatchMap().AddEntry(coverage1, 1, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage2{15, 16, 17};
  table.GetPatchMap().AddEntry(coverage2, 2, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage3{25, 26, 27};
  table.GetPatchMap().AddEntry(coverage3, 3, IFTB_ENCODING);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + uri_template.length() +
                3 * (min_entry_size + min_codepoints_size + 2) +
                1 /* non default encoding */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_NonDefaultPatchEncodingWithExtFiltering) {
  IFTTable table;

  PatchMap::Entry entry1({1, 2, 3}, 1, SHARED_BROTLI_ENCODING, false);
  table.GetPatchMap().AddEntry(entry1.coverage, entry1.patch_index,
                               entry1.encoding, entry1.extension_entry);

  PatchMap::Entry entry2({15, 16, 17}, 2, SHARED_BROTLI_ENCODING, false);
  table.GetPatchMap().AddEntry(entry2.coverage, entry2.patch_index,
                               entry2.encoding, entry2.extension_entry);

  PatchMap::Entry entry3({25, 26, 27}, 3, IFTB_ENCODING, false);
  table.GetPatchMap().AddEntry(entry3.coverage, entry3.patch_index,
                               entry3.encoding, entry3.extension_entry);

  // These ext entries shouldn't influence the default encoding selection.
  table.GetPatchMap().AddEntry(entry3.coverage, 4, IFTB_ENCODING, true);
  table.GetPatchMap().AddEntry(entry3.coverage, 5, IFTB_ENCODING, true);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + uri_template.length() +
                3 * (min_entry_size + min_codepoints_size + 1) + 3 +
                1 /* non default encoding */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_THAT(out.GetPatchMap().GetEntries(),
              UnorderedElementsAre(entry1, entry2, entry3));
}

TEST_F(Format2PatchMapTest, RoundTrip_IndexDeltas) {
  IFTTable table;
  PatchMap::Coverage coverage1{1, 2, 3};
  table.GetPatchMap().AddEntry(coverage1, 7, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage2{15, 16, 17};
  table.GetPatchMap().AddEntry(coverage2, 4, SHARED_BROTLI_ENCODING);

  PatchMap::Coverage coverage3{25, 26, 27};
  table.GetPatchMap().AddEntry(coverage3, 10, SHARED_BROTLI_ENCODING);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  ASSERT_EQ(encoded->length(),
            min_header_size + uri_template.length() +
                3 * (min_entry_size + min_codepoints_size + 1) + 3 +
                3 * 2 /* index deltas */);

  IFTTable out;
  auto s = Format2PatchMap::Deserialize(*encoded, out, false);
  ASSERT_TRUE(s.ok()) << s;

  EXPECT_EQ(out, table);
}

TEST_F(Format2PatchMapTest, RoundTrip_FilterExtensionEntries) {
  // TODO(garretrieger): once we support a separate non-ext and ext id in
  // IFTTable
  //                     update to test the correct serialization of it.
  IFTTable table;
  PatchMap::Entry entry1({1, 2, 3}, 1, SHARED_BROTLI_ENCODING, true);
  table.GetPatchMap().AddEntry(entry1.coverage, entry1.patch_index,
                               entry1.encoding, entry1.extension_entry);

  PatchMap::Entry entry2({5, 6, 7}, 2, SHARED_BROTLI_ENCODING, false);
  table.GetPatchMap().AddEntry(entry2.coverage, entry2.patch_index,
                               entry2.encoding, entry2.extension_entry);

  PatchMap::Entry entry3({9, 10, 11}, 3, SHARED_BROTLI_ENCODING, true);
  table.GetPatchMap().AddEntry(entry3.coverage, entry3.patch_index,
                               entry3.encoding, entry3.extension_entry);

  std::string uri_template = "foo/$1";
  std::string ext_uri_template = "ext/$1";
  table.SetUrlTemplate(uri_template);
  table.SetExtensionUrlTemplate(ext_uri_template);

  auto encoded = Format2PatchMap::Serialize(table, false);
  auto encoded_extensions = Format2PatchMap::Serialize(table, true);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  ASSERT_TRUE(encoded_extensions.ok()) << encoded_extensions.status();

  ASSERT_EQ(encoded->length(), min_header_size + uri_template.length() +
                                   min_entry_size + min_codepoints_size + 1 +
                                   2 /* index delta */);

  ASSERT_EQ(encoded_extensions->length(),
            min_header_size + ext_uri_template.length() +
                2 * (min_entry_size + min_codepoints_size + 1) +
                2 /* index delta */);

  // No extensions
  {
    IFTTable out;
    auto s = Format2PatchMap::Deserialize(*encoded, out, false);
    ASSERT_TRUE(s.ok()) << s;
    EXPECT_EQ(out.GetUrlTemplate(), uri_template);
    EXPECT_EQ(out.GetExtensionUrlTemplate(), uri_template);
    std::vector<PatchMap::Entry> expected1{entry2};
    EXPECT_EQ(out.GetPatchMap().GetEntries(), expected1);
  }

  // With Extensions
  {
    IFTTable out;
    auto s = Format2PatchMap::Deserialize(*encoded_extensions, out, true);
    ASSERT_TRUE(s.ok()) << s;
    EXPECT_EQ(out.GetUrlTemplate(), "");
    EXPECT_EQ(out.GetExtensionUrlTemplate(), ext_uri_template);
    std::vector<PatchMap::Entry> expected2{entry1, entry3};
    EXPECT_EQ(out.GetPatchMap().GetEntries(), expected2);
  }

  // Deserialize Both
  {
    IFTTable out;
    auto s = Format2PatchMap::Deserialize(*encoded, out, false);
    s.Update(Format2PatchMap::Deserialize(*encoded_extensions, out, true));
    ASSERT_TRUE(s.ok()) << s;

    EXPECT_EQ(out.GetExtensionUrlTemplate(), table.GetExtensionUrlTemplate());
    EXPECT_EQ(out.GetUrlTemplate(), table.GetUrlTemplate());
    EXPECT_THAT(out.GetPatchMap().GetEntries(),
                UnorderedElementsAre(entry1, entry2, entry3));
  }
}

}  // namespace ift::proto