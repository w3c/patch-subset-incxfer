#include "ift/feature_registry/feature_registry.h"

#include "gtest/gtest.h"

namespace ift::feature_registry {

class FeatureRegistryTest : public ::testing::Test {
 protected:
  FeatureRegistryTest() {}
};

TEST_F(FeatureRegistryTest, FeatureTagToIndex) {
  // Should match the feature registry in the specification found here:
  // https://w3c.github.io/IFT/Overview.html#feature-tag-list
  //
  // Spot check a few entries to make sure things seem to line up.
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('a', 'b', 'v', 'f')), 1);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('c', 'u', 'r', 's')), 15);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('a', 'a', 'l', 't')), 66);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('l', 'n', 'u', 'm')), 90);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('s', 's', '0', '1')), 123);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('s', 's', '1', '2')), 134);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('s', 's', '2', '0')), 142);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('c', 'v', '0', '1')), 143);
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('c', 'v', '9', '9')), 241);
}

TEST_F(FeatureRegistryTest, IndexToFeatureTag) {
  // Should match the feature registry in the specification found here:
  // https://w3c.github.io/IFT/Overview.html#feature-tag-list
  //
  // Spot check a few entries to make sure things seem to line up.
  ASSERT_EQ(IndexToFeatureTag(1), HB_TAG('a', 'b', 'v', 'f'));
  ASSERT_EQ(IndexToFeatureTag(15), HB_TAG('c', 'u', 'r', 's'));
  ASSERT_EQ(IndexToFeatureTag(66), HB_TAG('a', 'a', 'l', 't'));
  ASSERT_EQ(IndexToFeatureTag(90), HB_TAG('l', 'n', 'u', 'm'));
  ASSERT_EQ(IndexToFeatureTag(123), HB_TAG('s', 's', '0', '1'));
  ASSERT_EQ(IndexToFeatureTag(134), HB_TAG('s', 's', '1', '2'));
  ASSERT_EQ(IndexToFeatureTag(142), HB_TAG('s', 's', '2', '0'));
  ASSERT_EQ(IndexToFeatureTag(143), HB_TAG('c', 'v', '0', '1'));
  ASSERT_EQ(IndexToFeatureTag(241), HB_TAG('c', 'v', '9', '9'));
}

TEST_F(FeatureRegistryTest, FeatureTagToIndex_NotFound) {
  ASSERT_EQ(FeatureTagToIndex(HB_TAG('x', 'x', 'x', 'x')),
            HB_TAG('x', 'x', 'x', 'x'));
}

TEST_F(FeatureRegistryTest, IndexToFeatureTag_NotFound) {
  ASSERT_EQ(IndexToFeatureTag(HB_TAG('x', 'x', 'x', 'x')),
            HB_TAG('x', 'x', 'x', 'x'));
}

}  // namespace ift::feature_registry
