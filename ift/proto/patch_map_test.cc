#include "ift/proto/patch_map.h"

#include <cstdio>
#include <cstring>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "gtest/gtest.h"
#include "ift/proto/patch_encoding.h"

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StrCat;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;

namespace ift::proto {

class PatchMapTest : public ::testing::Test {
 protected:
  PatchMapTest() {}
};

TEST_F(PatchMapTest, GetEntries) {
  PatchMap::Entry entries[] = {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
  };

  PatchMap map {entries[0], entries[1]};
  Span<const PatchMap::Entry> expected(entries);

  ASSERT_EQ(map.GetEntries(), expected);
}

TEST_F(PatchMapTest, Equal) {
  PatchMap map1 {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
  };

  PatchMap map2 {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
  };

  PatchMap map3 {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 3, GLYPH_KEYED},
  };

  PatchMap map4 {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, TABLE_KEYED_FULL},
  };

  PatchMap map5 {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 58}, 2, GLYPH_KEYED},
  };

  ASSERT_EQ(map1, map2);
  ASSERT_FALSE(map1 == map3);
  ASSERT_NE(map1, map3);
  ASSERT_NE(map1, map4);
  ASSERT_NE(map1, map5);
}

TEST_F(PatchMapTest, AddEntry) {
  PatchMap map {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
  };
  
  map.AddEntry({77, 79, 80}, 5, TABLE_KEYED_FULL);

  PatchMap expected = {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
      {{77, 79, 80}, 5, TABLE_KEYED_FULL},
  };

  ASSERT_EQ(map, expected);

  map.AddEntry({1, 2, 3}, 3, GLYPH_KEYED);

  expected = {
      {{30, 32}, 1, TABLE_KEYED_FULL},
      {{55, 56, 57}, 2, GLYPH_KEYED},
      {{77, 79, 80}, 5, TABLE_KEYED_FULL},
      {{1, 2, 3}, 3, GLYPH_KEYED},
  };

  ASSERT_EQ(map, expected);
}

TEST_F(PatchMapTest, IsInvalidating) {
  ASSERT_FALSE(PatchMap::Entry({}, 0, GLYPH_KEYED).IsInvalidating());
  ASSERT_TRUE(PatchMap::Entry({}, 0, TABLE_KEYED_FULL).IsInvalidating());
  ASSERT_TRUE(
      PatchMap::Entry({}, 0, TABLE_KEYED_PARTIAL).IsInvalidating());
}


TEST_F(PatchMapTest, CoverageIntersection) {
  // important cases:
  // - input unspecified vs coverage specified
  // - input specified vs coverage specified
  // - input specified vs coverage unspecified
  // - input unspecified vs coverage unspecified
  PatchMap::Coverage codepoints({1, 2, 3});
  PatchMap::Coverage codepoints_features({1, 2, 3});
  codepoints_features.features = {HB_TAG('a', 'b', 'c', 'd')};
  PatchMap::Coverage features;
  features.features = {HB_TAG('a', 'b', 'c', 'd')};
  PatchMap::Coverage empty;

  PatchMap::Coverage design_space;
  design_space.design_space[HB_TAG('w', 'g', 'h', 't')] =
      *common::AxisRange::Range(100, 300);
  design_space.design_space[HB_TAG('w', 'd', 't', 'h')] =
      *common::AxisRange::Range(50, 100);

  flat_hash_set<uint32_t> codepoints_in_match = {2, 7};
  flat_hash_set<uint32_t> codepoints_in_no_match = {5};
  btree_set<uint32_t> features_in_match = {HB_TAG('a', 'b', 'c', 'd'),
                                               HB_TAG('y', 'y', 'y', 'y')};
  btree_set<uint32_t> features_in_no_match = {HB_TAG('x', 'x', 'x', 'x')};

  flat_hash_map<hb_tag_t, common::AxisRange> design_space_match = {
      {HB_TAG('w', 'g', 'h', 't'), common::AxisRange::Point(200)},
  };
  flat_hash_map<hb_tag_t, common::AxisRange> design_space_no_match_1 = {
      {HB_TAG('w', 'g', 'h', 't'), common::AxisRange::Point(500)},
  };
  flat_hash_map<hb_tag_t, common::AxisRange> design_space_no_match_2 = {
      {HB_TAG('x', 'x', 'x', 'x'), common::AxisRange::Point(500)},
  };

  flat_hash_set<uint32_t> unspecified_codepoints;
  btree_set<uint32_t> unspecified_features;
  flat_hash_map<hb_tag_t, common::AxisRange> unspecified_design_space;

  ASSERT_FALSE(codepoints.Intersects(unspecified_codepoints, unspecified_features,
                                     unspecified_design_space));
  ASSERT_FALSE(codepoints_features.Intersects(unspecified_codepoints, unspecified_features,
                                              unspecified_design_space));
  ASSERT_FALSE(features.Intersects(unspecified_codepoints, unspecified_features,
                                   unspecified_design_space));
  ASSERT_TRUE(empty.Intersects(unspecified_codepoints, unspecified_features,
                               unspecified_design_space));

  ASSERT_TRUE(codepoints.Intersects(codepoints_in_match, unspecified_features,
                                    unspecified_design_space));
  ASSERT_TRUE(codepoints.Intersects(codepoints_in_match, features_in_match,
                                    unspecified_design_space));
  ASSERT_TRUE(codepoints.Intersects(codepoints_in_match, features_in_no_match,
                                    unspecified_design_space));
  ASSERT_FALSE(codepoints.Intersects(codepoints_in_no_match, unspecified_features,
                                     unspecified_design_space));
  ASSERT_FALSE(codepoints.Intersects(codepoints_in_no_match, features_in_match,
                                     unspecified_design_space));
  ASSERT_FALSE(codepoints.Intersects(
      codepoints_in_no_match, features_in_no_match, unspecified_design_space));

  ASSERT_TRUE(features.Intersects(unspecified_codepoints, features_in_match,
                                  unspecified_design_space));
  ASSERT_TRUE(features.Intersects(codepoints_in_match, features_in_match,
                                  unspecified_design_space));
  ASSERT_TRUE(features.Intersects(codepoints_in_no_match, features_in_match,
                                  unspecified_design_space));
  ASSERT_FALSE(features.Intersects(unspecified_codepoints, features_in_no_match,
                                   unspecified_design_space));
  ASSERT_FALSE(features.Intersects(codepoints_in_match, features_in_no_match,
                                   unspecified_design_space));
  ASSERT_FALSE(features.Intersects(codepoints_in_no_match, features_in_no_match,
                                   unspecified_design_space));

  ASSERT_TRUE(codepoints_features.Intersects(
      codepoints_in_match, features_in_match, unspecified_design_space));
  ASSERT_FALSE(codepoints_features.Intersects(unspecified_codepoints, features_in_match,
                                              unspecified_design_space));
  ASSERT_TRUE(codepoints_features.Intersects(
      codepoints_in_match, features_in_match, design_space_no_match_1));
  ASSERT_FALSE(codepoints_features.Intersects(
      codepoints_in_match, unspecified_features, unspecified_design_space));
  ASSERT_FALSE(codepoints_features.Intersects(
      codepoints_in_no_match, features_in_match, unspecified_design_space));
  ASSERT_FALSE(codepoints_features.Intersects(
      codepoints_in_match, features_in_no_match, unspecified_design_space));

  ASSERT_TRUE(design_space.Intersects(unspecified_codepoints, unspecified_features,
                                      design_space_match));
  ASSERT_FALSE(design_space.Intersects(unspecified_codepoints, unspecified_features,
                                       design_space_no_match_1));
  ASSERT_FALSE(design_space.Intersects(unspecified_codepoints, unspecified_features,
                                       design_space_no_match_2));
}

}  // namespace ift::proto
