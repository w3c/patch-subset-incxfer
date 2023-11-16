#include "absl/status/status.h"
#include "absl/types/span.h"
#include "common/file_font_provider.h"
#include "common/font_provider.h"
#include "gtest/gtest.h"
#include "patch_subset/vcdiff_binary_diff.h"
#include "patch_subset/vcdiff_binary_patch.h"

namespace patch_subset {

using absl::Span;
using absl::Status;
using common::BinaryDiff;
using common::BinaryPatch;
using common::FileFontProvider;
using common::FontData;
using common::FontProvider;

class VCDIFFPatchingTest : public ::testing::Test {
 protected:
  VCDIFFPatchingTest()
      : font_provider_(new FileFontProvider("patch_subset/testdata/")),
        diff_(new VCDIFFBinaryDiff()),
        patch_(new VCDIFFBinaryPatch()) {}

  ~VCDIFFPatchingTest() override {}

  void SetUp() override {
    EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Meows.ttf", &subset_a_),
              absl::OkStatus());
    EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Awesome.ttf", &subset_b_),
              absl::OkStatus());
    EXPECT_GT(subset_a_.size(), 0);
    EXPECT_GT(subset_b_.size(), 0);
  }

  std::unique_ptr<FontProvider> font_provider_;
  std::unique_ptr<BinaryDiff> diff_;
  std::unique_ptr<BinaryPatch> patch_;
  FontData subset_a_;
  FontData subset_b_;
};

TEST_F(VCDIFFPatchingTest, DiffAndPatchPatchWithEmptyBase) {
  FontData empty;
  FontData patch;
  EXPECT_EQ(diff_->Diff(empty, subset_a_, &patch), absl::OkStatus());

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(empty, patch, &patched), absl::OkStatus());
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_a_));
}

TEST_F(VCDIFFPatchingTest, DiffAndPatch) {
  FontData patch;
  EXPECT_EQ(diff_->Diff(subset_a_, subset_b_, &patch), absl::OkStatus());

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_LT(patch.size(), subset_b_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_b_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(subset_a_, patch, &patched), absl::OkStatus());
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_b_));
}

}  // namespace patch_subset
