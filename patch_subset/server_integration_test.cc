#include "gtest/gtest.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/codepoint_mapping_checksum.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/harfbuzz_subsetter.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/noop_codepoint_predictor.h"
#include "patch_subset/patch_subset_server_impl.h"

using ::absl::string_view;

namespace patch_subset {

class PatchSubsetServerIntegrationTest : public ::testing::Test {
 protected:
  PatchSubsetServerIntegrationTest()
      : font_provider_(new FileFontProvider("patch_subset/testdata/")),
        binary_diff_(new BrotliBinaryDiff()),
        server_(
            0, std::unique_ptr<FontProvider>(font_provider_),
            std::unique_ptr<Subsetter>(new HarfbuzzSubsetter()),
            std::unique_ptr<BinaryDiff>(binary_diff_),
            std::unique_ptr<Hasher>(new FastHasher()),
            std::unique_ptr<CodepointMapper>(nullptr),
            std::unique_ptr<CodepointMappingChecksum>(nullptr),
            std::unique_ptr<CodepointPredictor>(new NoopCodepointPredictor())) {
    font_provider_->GetFont("Roboto-Regular.abcd.ttf", &roboto_abcd_);
    font_provider_->GetFont("Roboto-Regular.ab.ttf", &roboto_ab_);

    FontData roboto_regular;
    font_provider_->GetFont("Roboto-Regular.ttf", &roboto_regular);

    FastHasher hasher;
    original_font_fingerprint_ = hasher.Checksum(roboto_regular.str());
    subset_ab_fingerprint_ = hasher.Checksum(roboto_ab_.str());
    subset_abcd_fingerprint_ = hasher.Checksum(roboto_abcd_.str());
  }

  void CheckPatch(const FontData& base, const FontData& target,
                  string_view patch_string) {
    // Check that diff base and target produces patch,
    // and that applying patch to base produces target.
    BrotliBinaryDiff binary_diff;
    FontData expected_patch;
    EXPECT_EQ(binary_diff.Diff(base, target, &expected_patch), StatusCode::kOk);
    EXPECT_EQ(patch_string, expected_patch.str());

    FontData actual_target;
    FontData patch;
    BrotliBinaryPatch binary_patch;
    patch.copy(patch_string.data(), patch_string.size());
    EXPECT_EQ(binary_patch.Patch(base, patch, &actual_target), StatusCode::kOk);
    EXPECT_EQ(actual_target.str(), target.str());
  }

  FontProvider* font_provider_;
  BinaryDiff* binary_diff_;
  PatchSubsetServerImpl server_;

  FontData empty_;
  FontData roboto_abcd_;
  FontData roboto_ab_;

  uint64_t original_font_fingerprint_;
  uint64_t subset_ab_fingerprint_;
  uint64_t subset_abcd_fingerprint_;
};

TEST_F(PatchSubsetServerIntegrationTest, NewRequest) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequestProto request;
  CompressedSet::Encode(*set_abcd, request.mutable_codepoints_needed());

  PatchResponseProto response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  EXPECT_EQ(response.original_font_fingerprint(), original_font_fingerprint_);
  EXPECT_EQ(response.patched_fingerprint(), subset_abcd_fingerprint_);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(empty_, roboto_abcd_, response.replacement());
}

TEST_F(PatchSubsetServerIntegrationTest, PatchRequest) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequestProto request;
  CompressedSet::Encode(*set_ab, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd, request.mutable_codepoints_needed());
  request.set_original_font_fingerprint(original_font_fingerprint_);
  request.set_base_fingerprint(subset_ab_fingerprint_);

  PatchResponseProto response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  EXPECT_EQ(response.original_font_fingerprint(), original_font_fingerprint_);
  EXPECT_EQ(response.patched_fingerprint(), subset_abcd_fingerprint_);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(roboto_ab_, roboto_abcd_, response.patch());
}

TEST_F(PatchSubsetServerIntegrationTest, BadOriginalFingerprint) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequestProto request;
  CompressedSet::Encode(*set_ab, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd, request.mutable_codepoints_needed());
  request.set_original_font_fingerprint(0);
  request.set_base_fingerprint(subset_ab_fingerprint_);

  PatchResponseProto response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  EXPECT_EQ(response.original_font_fingerprint(), original_font_fingerprint_);
  EXPECT_EQ(response.patched_fingerprint(), subset_abcd_fingerprint_);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(empty_, roboto_abcd_, response.replacement());
}

TEST_F(PatchSubsetServerIntegrationTest, BadBaseFingerprint) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequestProto request;
  CompressedSet::Encode(*set_ab, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd, request.mutable_codepoints_needed());
  request.set_original_font_fingerprint(original_font_fingerprint_);
  request.set_base_fingerprint(0);

  PatchResponseProto response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  EXPECT_EQ(response.original_font_fingerprint(), original_font_fingerprint_);
  EXPECT_EQ(response.patched_fingerprint(), subset_abcd_fingerprint_);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(empty_, roboto_abcd_, response.replacement());
}

}  // namespace patch_subset
