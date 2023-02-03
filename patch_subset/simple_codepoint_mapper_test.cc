#include "patch_subset/simple_codepoint_mapper.h"

#include "gtest/gtest.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/hb_set_unique_ptr.h"

namespace patch_subset {

using absl::StatusCode;

class SimpleCodepointMapperTest : public ::testing::Test {
 protected:
  SimpleCodepointMapperTest() {}

  SimpleCodepointMapper codepoint_mapper_;

  void CheckMapping(const CodepointMap& map, hb_codepoint_t from,
                    hb_codepoint_t to) {
    hb_codepoint_t cp = from;
    EXPECT_EQ(absl::OkStatus(), map.Encode(&cp));
    EXPECT_EQ(cp, to);
  }
};

TEST_F(SimpleCodepointMapperTest, MapCodepoints) {
  hb_set_unique_ptr codepoints = make_hb_set(4, 1, 7, 8, 11);

  CodepointMap mapping;
  codepoint_mapper_.ComputeMapping(codepoints.get(), &mapping);

  CheckMapping(mapping, 1, 0);
  CheckMapping(mapping, 7, 1);
  CheckMapping(mapping, 8, 2);
  CheckMapping(mapping, 11, 3);
}

}  // namespace patch_subset
