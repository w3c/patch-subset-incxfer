#include "ift/feature_registry/feature_registry.h"

#include "absl/status/statusor.h"
#include "gtest/gtest.h"

using absl::StatusOr;

namespace ift::feature_registry {

class FeatureRegistryTest : public ::testing::Test {
 protected:
  FeatureRegistryTest() {}
};

TEST_F(FeatureRegistryTest, SpotCheckEntries) {
  // Should match the feature registry in the specification found here:
  // https://w3c.github.io/IFT/Overview.html#feature-tag-list
  //
  // Spot check a few entries to make sure things seem to line up.
  auto index = FeatureTagToIndex(HB_TAG('a', 'b', 'v', 'f'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 1);

  index = FeatureTagToIndex(HB_TAG('c', 'u', 'r', 's'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 15);

  index = FeatureTagToIndex(HB_TAG('a', 'a', 'l', 't'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 66);

  index = FeatureTagToIndex(HB_TAG('l', 'n', 'u', 'm'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 90);

  index = FeatureTagToIndex(HB_TAG('s', 's', '0', '1'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 123);

  index = FeatureTagToIndex(HB_TAG('s', 's', '1', '2'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 134);

  index = FeatureTagToIndex(HB_TAG('s', 's', '2', '0'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 142);

  index = FeatureTagToIndex(HB_TAG('c', 'v', '0', '1'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 143);

  index = FeatureTagToIndex(HB_TAG('c', 'v', '9', '9'));
  ASSERT_TRUE(index.ok()) << index.status();
  ASSERT_EQ(*index, 241);
}

TEST_F(FeatureRegistryTest, NotFound) {
  auto index = FeatureTagToIndex(HB_TAG('x', 'x', 'x', 'x'));
  ASSERT_TRUE(absl::IsNotFound(index.status())) << index.status();
}

}  // namespace ift::feature_registry
