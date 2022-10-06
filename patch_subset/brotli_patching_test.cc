#include "absl/types/span.h"
#include "common/status.h"
#include "gtest/gtest.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/font_provider.h"

using ::absl::Span;

namespace patch_subset {

class BrotliPatchingTest : public ::testing::Test {
 protected:
  BrotliPatchingTest()
      : font_provider_(new FileFontProvider("patch_subset/testdata/")),
        diff_(new BrotliBinaryDiff()),
        patch_(new BrotliBinaryPatch()) {}

  ~BrotliPatchingTest() override {}

  void SetUp() override {
    EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Meows.ttf", &subset_a_),
              StatusCode::kOk);
    EXPECT_EQ(font_provider_->GetFont("Roboto-Regular.Awesome.ttf", &subset_b_),
              StatusCode::kOk);
    EXPECT_GT(subset_a_.size(), 0);
    EXPECT_GT(subset_b_.size(), 0);
  }

  std::unique_ptr<FontProvider> font_provider_;
  std::unique_ptr<BrotliBinaryDiff> diff_;
  std::unique_ptr<BinaryPatch> patch_;
  FontData subset_a_;
  FontData subset_b_;
};

TEST_F(BrotliPatchingTest, DiffAndPatchPatchWithEmptyBase) {
  FontData empty;
  FontData patch;
  EXPECT_EQ(diff_->Diff(empty, subset_a_, &patch), StatusCode::kOk);

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(empty, patch, &patched), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_a_));
}

TEST_F(BrotliPatchingTest, DiffAndPatch) {
  FontData patch;
  EXPECT_EQ(diff_->Diff(subset_a_, subset_b_, &patch), StatusCode::kOk);

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_LT(patch.size(), subset_b_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_b_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(subset_a_, patch, &patched), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_b_));
}

TEST_F(BrotliPatchingTest, StitchingWithEmptyBase) {
  FontData empty;

  std::vector<uint8_t> sink;
  EXPECT_EQ(diff_->Diff(empty, subset_a_.str(0, 1000), 0, false, sink),
            StatusCode::kOk);

  EXPECT_EQ(diff_->Diff(empty, subset_a_.str(1000), 1000, true, sink),
            StatusCode::kOk);

  FontData patch;
  patch.copy(reinterpret_cast<const char*>(sink.data()), sink.size());

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(empty, patch, &patched), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_a_));
}

TEST_F(BrotliPatchingTest, StitchingWithBase) {
  std::vector<uint8_t> sink;
  EXPECT_EQ(diff_->Diff(subset_a_, subset_b_.str(0, 1000), 0, false, sink),
            StatusCode::kOk);

  EXPECT_EQ(diff_->Diff(subset_a_, subset_b_.str(1000), 1000, true, sink),
            StatusCode::kOk);

  FontData patch;
  patch.copy(reinterpret_cast<const char*>(sink.data()), sink.size());

  EXPECT_GT(patch.size(), 0);
  EXPECT_LT(patch.size(), subset_a_.size());
  EXPECT_LT(patch.size(), subset_b_.size());
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_a_));
  EXPECT_NE(Span<const char>(patch), Span<const char>(subset_b_));

  FontData patched;
  EXPECT_EQ(patch_->Patch(subset_a_, patch, &patched), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(patched), Span<const char>(subset_b_));
}

}  // namespace patch_subset
