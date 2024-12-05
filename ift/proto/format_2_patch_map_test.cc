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

static std::string HeaderSimple() {
  return {
      0x02,                    // format
      0x00, 0x00, 0x00, 0x00,  // reserved
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,  // compat id
      0x01,                                // default format = Table Keyed Full
      0x00, 0x00, 0x01,                    // entry count
      0x00, 0x00, 0x00, 0x29,              // entries offset
      0x00, 0x00, 0x00, 0x00,              // entry id string data offset
      0x00, 0x06,                          // uri template length
      0x66, 0x6f, 0x6f, 0x2f, 0x24, 0x31,  // foo/$1
  };
}

TEST_F(Format2PatchMapTest, Simple) {
  IFTTable table;
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{1, 2, 3};
  map.AddEntry(coverage, 1, TABLE_KEYED_FULL);

  table.SetUrlTemplate("foo/$1");
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
      // entry 0
      0x10,                   // format = 00010000 = Codepoints
      0b00000101, 0b00001110  // codepoints (BF4, depth 1)= {1, 2, 3}
  };
  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, TwoByteBias) {
  IFTTable table;
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{10251, 10252, 10253};
  map.AddEntry(coverage, 1, TABLE_KEYED_FULL);

  table.SetUrlTemplate("foo/$1");
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
      // entry 0
      0x20,                   // 0b0010 0000 = Codepoints w/ u16 bias
      0x28, 0x0b,             // bias = 10251
      0b00000101, 0b00000111  // codepoints (BF4, depth 1)= {0, 1, 2}
  };

  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, ThreeByteBias) {
  IFTTable table;
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{100251, 100252, 100253};
  map.AddEntry(coverage, 1, TABLE_KEYED_FULL);

  table.SetUrlTemplate("foo/$1");
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
      // entry 0
      0x30,  // 0b0010 0000 = Codepoints w/ u16 bias
      0x01,       (char)0x87, (char)0x9B,  // bias = 100251
      0b00000101, 0b00000111  // codepoints (BF4, depth 1)= {0, 1, 2}
  };

  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, ComplexSet) {
  IFTTable table;
  PatchMap& map = table.GetPatchMap();
  PatchMap::Coverage coverage{123, 155, 179, 180, 181, 182, 1013};
  map.AddEntry(coverage, 1, TABLE_KEYED_FULL);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
     0x10,  // 0b0001 0000 = Codepoints
     0b00010101, // BF4, depth = 5
     0x69, (char) 0x88, (char) 0x8a, 0x44, 0x23, (char) 0x88, 0x78, 0x02,
  };

  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, Features) {
  IFTTable table;

  PatchMap::Coverage coverage{1, 2, 3};
  coverage.features.insert(HB_TAG('w', 'g', 'h', 't'));
  coverage.features.insert(HB_TAG('w', 'd', 't', 'h'));
  table.GetPatchMap().AddEntry(coverage, 1, TABLE_KEYED_FULL);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
    0x11, // 0b00010001 = Features and Codepoints
    0x02, // feature count
    0x77, 0x64, 0x74, 0x68, // wdth
    0x77, 0x67, 0x68, 0x74, // wght
    0x00, 0x00, // design space count
    0x05, 0x0e,
  };

  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, DesignSpace) {
  IFTTable table;

  PatchMap::Coverage coverage{1, 2, 3};
  coverage.design_space[HB_TAG('w', 'g', 'h', 't')] =
      *common::AxisRange::Range(100.0f, 200.0f);
  coverage.design_space[HB_TAG('w', 'd', 't', 'h')] =
      common::AxisRange::Point(0.75f);
  table.GetPatchMap().AddEntry(coverage, 1, TABLE_KEYED_FULL);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string entry_0 = {
    0x11, // 0b00010001 = Features and Codepoints
    0x00, // feature count
    0x00, 0x02, // design space count
    0x77, 0x64, 0x74, 0x68,        // tag = 'wdth'
    0x00, 0x00, (char) 0xc0, 0x00, // start
    0x00, 0x00, (char) 0xc0, 0x00, // end
    0x77, 0x67, 0x68, 0x74,        // tag = 'wght'
    0x00, 0x64, 0x00, 0x00,        // start
    0x00, (char) 0xc8, 0x00, 0x00, // end
    0x05, 0x0e,
  };

  ASSERT_EQ(*encoded, absl::StrCat(HeaderSimple(), entry_0));
}

TEST_F(Format2PatchMapTest, NonDefaultPatchFormat) {
  IFTTable table;

  PatchMap::Coverage coverage1{1, 2, 3};
  table.GetPatchMap().AddEntry(coverage1, 1, TABLE_KEYED_PARTIAL);

  PatchMap::Coverage coverage2{15, 16, 17};
  table.GetPatchMap().AddEntry(coverage2, 2, TABLE_KEYED_PARTIAL);

  PatchMap::Coverage coverage3{25, 26, 27};
  table.GetPatchMap().AddEntry(coverage3, 3, GLYPH_KEYED);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  table.SetId({2, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string header = {
    0x02, // format
    0x00, 0x00, 0x00, 0x00, // reserved
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x04, // compat id
    
    0x02, // default format (Table Keyed Partial)
    0x00, 0x00,  0x03, // entry count
    0x00, 0x00, 0x00, 0x29, // entries
    0x00, 0x00, 0x00, 0x00, // id string data
    0x00, 0x06,
    0x66, 0x6f, 0x6f, 0x2f, 0x24,  0x31 // uri template
  };

  std::string entry_0 = {
    0x10, // Codepoints
    0x05, 0x0e, // codepoints = {1, 2, 3}
  };

  std::string entry_1 = {
    0x10, // codepoints
    0b00001101, // BF4, depth = 3
    (char) 0b10000011, (char) 0b10000001, 0x03, // codepoints = {15, 16, 17}
  };

  std::string entry_2 = {
    0x18, // format = Codepoints and format
    0x03, // format = Glyph Keyed,
    0x0d, 0x42, 0x0e,  // codepoints = {25, 26, 27}
  };

  ASSERT_EQ(*encoded, absl::StrCat(header, entry_0, entry_1, entry_2));
}

TEST_F(Format2PatchMapTest, IndexDeltas) {
  IFTTable table;
  PatchMap::Coverage coverage1{1, 2, 3};
  table.GetPatchMap().AddEntry(coverage1, 7, TABLE_KEYED_FULL);

  PatchMap::Coverage coverage2{15, 16, 17};
  table.GetPatchMap().AddEntry(coverage2, 4, TABLE_KEYED_FULL);

  PatchMap::Coverage coverage3{25, 26, 27};
  table.GetPatchMap().AddEntry(coverage3, 10, TABLE_KEYED_FULL);

  std::string uri_template = "foo/$1";
  table.SetUrlTemplate(uri_template);
  table.SetId({1, 2, 3, 4});

  auto encoded = Format2PatchMap::Serialize(table);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  std::string header = {
    0x02, // format
    0x00, 0x00, 0x00, 0x00, // reserved
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x04, // compat id
    
    0x01, // default format (Table Keyed Full)
    0x00, 0x00,  0x03, // entry count
    0x00, 0x00, 0x00, 0x29, // entries
    0x00, 0x00, 0x00, 0x00, // id string data
    0x00, 0x06,
    0x66, 0x6f, 0x6f, 0x2f, 0x24,  0x31 // uri template
  };

  std::string entry_0 = {
    0x14, // format = Codepoints | ID delta
    0x00, 0x00, 0x06, // ID delta +6 -> 7
    0x05, 0x0e,       // codepoints = {1, 2, 3}
  };

  std::string entry_1 = {
    0x14, // format = Codepoints | ID delta
    (char) 0xff, (char) 0xff, (char) 0xfc, // ID delta -4 -> 4    
    0x0d, (char) 0x83, (char) 0x81, 0x03,  // codepoints = {15, 16, 17}
  };

  std::string entry_2 = {
    0x14, // format = Codepoints | ID delta
    0x00, 0x00, 0x05, // ID delta = +5 -> 10
    0x0d, 0x42, 0x0e, // codepoints = {25, 26, 27}
  };

  ASSERT_EQ(*encoded, absl::StrCat(header, entry_0, entry_1, entry_2));
}

}  // namespace ift::proto