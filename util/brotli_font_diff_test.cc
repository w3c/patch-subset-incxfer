#include "util/brotli_font_diff.h"

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "patch_subset/brotli_binary_patch.h"
#include "hb-subset.h"

using ::absl::Span;
using ::patch_subset::BrotliBinaryPatch;
using ::util::BrotliFontDiff;

namespace patch_subset {

class BrotliFontDiffTest : public ::testing::Test {
 protected:
  BrotliFontDiffTest() {}

  ~BrotliFontDiffTest() override {}

  void SetUp() override {
  }
};

TEST_F(BrotliFontDiffTest, Diff) {
  hb_blob_t* font_data =
      hb_blob_create_from_file_or_fail("patch_subset/testdata/Roboto-Regular.ttf");
  ASSERT_TRUE(font_data);

  hb_face_t* face = hb_face_create(font_data, 0);
  hb_blob_destroy(font_data);

  hb_subset_input_t* input = hb_subset_input_create_or_fail();

  hb_set_add_range(hb_subset_input_unicode_set(input), 0x41, 0x5A);
  hb_subset_plan_t* base_plan = hb_subset_plan_create_or_fail(face, input);
  hb_face_t* base_face = hb_subset_plan_execute_or_fail(base_plan);
  hb_blob_t* base_data = hb_face_reference_blob(base_face);
  ASSERT_TRUE(base_plan);

  hb_set_add_range(hb_subset_input_unicode_set(input), 0x61, 0x7A);
  hb_subset_plan_t* derived_plan = hb_subset_plan_create_or_fail(face, input);
  hb_face_t* derived_face = hb_subset_plan_execute_or_fail(derived_plan);
  hb_blob_t* derived_data = hb_face_reference_blob(derived_face);
  ASSERT_TRUE(derived_plan);


  FontData base;
  base.copy(hb_blob_get_data(base_data, nullptr), hb_blob_get_length(base_data));
  FontData derived;
  derived.copy(hb_blob_get_data(derived_data, nullptr), hb_blob_get_length(derived_data));

  BrotliFontDiff differ;
  FontData patch;
  differ.Diff(base_plan, base_face, derived_plan, derived_face, &patch);

  BrotliBinaryPatch patcher;
  FontData patched;
  EXPECT_EQ(StatusCode::kOk,
            patcher.Patch(base, patch, &patched));

  EXPECT_EQ(derived.str(), patched.str());

  hb_subset_input_destroy(input);
  hb_subset_plan_destroy(base_plan);
  hb_subset_plan_destroy(derived_plan);
  hb_face_destroy(base_face);
  hb_face_destroy(derived_face);
  hb_blob_destroy(base_data);
  hb_blob_destroy(derived_data);
  hb_face_destroy(face);
}



}  // namespace patch_subset
