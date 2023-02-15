#include "gtest/gtest.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fast_hasher.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/harfbuzz_subsetter.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/noop_codepoint_predictor.h"
#include "patch_subset/patch_subset_server_impl.h"
#include "patch_subset/vcdiff_binary_diff.h"
#include "patch_subset/vcdiff_binary_patch.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using patch_subset::cbor::AxisInterval;
using patch_subset::cbor::AxisSpace;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;

class PatchSubsetServerIntegrationTest : public ::testing::Test {
 protected:
  PatchSubsetServerIntegrationTest()
      : font_provider_(new FileFontProvider("patch_subset/testdata/")),
        server_(
            0, std::unique_ptr<FontProvider>(font_provider_),
            std::unique_ptr<Subsetter>(new HarfbuzzSubsetter()),
            std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff()),
            std::unique_ptr<BinaryDiff>(new VCDIFFBinaryDiff()),
            std::unique_ptr<Hasher>(new FastHasher()),
            std::unique_ptr<CodepointMapper>(nullptr),
            std::unique_ptr<IntegerListChecksum>(nullptr),
            std::unique_ptr<CodepointPredictor>(new NoopCodepointPredictor())) {
    EXPECT_TRUE(
        font_provider_->GetFont("Roboto-Regular.abcd.ttf", &roboto_abcd_).ok());
    EXPECT_TRUE(
        font_provider_->GetFont("Roboto-Regular.ab.ttf", &roboto_ab_).ok());

    FontData roboto_regular;
    EXPECT_TRUE(
        font_provider_->GetFont("Roboto-Regular.ttf", &roboto_regular).ok());

    FastHasher hasher;
    original_font_checksum = hasher.Checksum(roboto_regular.str());
    subset_ab_checksum = hasher.Checksum(roboto_ab_.str());
    subset_abcd_checksum = hasher.Checksum(roboto_abcd_.str());
  }

  void CheckPatch(const FontData& base, const FontData& target,
                  string_view patch_string,
                  PatchFormat format = PatchFormat::BROTLI_SHARED_DICT) {
    std::unique_ptr<BinaryDiff> binary_diff;
    std::unique_ptr<BinaryPatch> binary_patch;
    switch (format) {
      case BROTLI_SHARED_DICT:
        binary_diff = std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff());
        binary_patch = std::unique_ptr<BinaryPatch>(new BrotliBinaryPatch());
        break;
      case VCDIFF:
        binary_diff = std::unique_ptr<BinaryDiff>(new VCDIFFBinaryDiff());
        binary_patch = std::unique_ptr<BinaryPatch>(new VCDIFFBinaryPatch());
        break;
    }

    // Check that diff base and target produces patch,
    // and that applying patch to base produces target.
    FontData expected_patch;
    EXPECT_EQ(binary_diff->Diff(base, target, &expected_patch),
              absl::OkStatus());
    EXPECT_EQ(patch_string, expected_patch.str());

    FontData actual_target;
    FontData patch;
    patch.copy(patch_string.data(), patch_string.size());
    EXPECT_EQ(binary_patch->Patch(base, patch, &actual_target),
              absl::OkStatus());
    EXPECT_EQ(actual_target.str(), target.str());
  }

  FontProvider* font_provider_;
  PatchSubsetServerImpl server_;

  FontData empty_;
  FontData roboto_abcd_;
  FontData roboto_ab_;

  uint64_t original_font_checksum;
  uint64_t subset_ab_checksum;
  uint64_t subset_abcd_checksum;
};

TEST_F(PatchSubsetServerIntegrationTest, NewRequest) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetAcceptFormats({PatchFormat::BROTLI_SHARED_DICT});
  request.SetProtocolVersion(ProtocolVersion::ONE);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);
  EXPECT_FALSE(response.HasSubsetAxisSpace());
  EXPECT_FALSE(response.HasOriginalAxisSpace());

  CheckPatch(empty_, roboto_abcd_, response.Replacement());
}

TEST_F(PatchSubsetServerIntegrationTest, NewRequest_Variable) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetAcceptFormats({PatchFormat::BROTLI_SHARED_DICT});
  request.SetProtocolVersion(ProtocolVersion::ONE);

  PatchResponse response;
  ASSERT_EQ(server_.Handle("Roboto[wdth,wght].ttf", request, response),
            absl::OkStatus());

  AxisSpace expected;
  expected.AddInterval(HB_TAG('w', 'g', 'h', 't'), AxisInterval(100, 900));
  expected.AddInterval(HB_TAG('w', 'd', 't', 'h'), AxisInterval(75, 100));

  EXPECT_EQ(response.SubsetAxisSpace(), expected);
  EXPECT_EQ(response.OriginalAxisSpace(), expected);
}

TEST_F(PatchSubsetServerIntegrationTest, NewRequestVCDIFF) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetAcceptFormats({PatchFormat::VCDIFF});
  request.SetProtocolVersion(ProtocolVersion::ONE);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::VCDIFF);

  CheckPatch(empty_, roboto_abcd_, response.Replacement(), PatchFormat::VCDIFF);
}

TEST_F(PatchSubsetServerIntegrationTest, PatchRequest) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  request.SetProtocolVersion(ProtocolVersion::ONE);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetAcceptFormats({PatchFormat::BROTLI_SHARED_DICT});
  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(subset_ab_checksum);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(roboto_ab_, roboto_abcd_, response.Patch());
}

TEST_F(PatchSubsetServerIntegrationTest, PatchRequestVCDIFF) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  request.SetProtocolVersion(ProtocolVersion::ONE);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetAcceptFormats({PatchFormat::VCDIFF});
  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(subset_ab_checksum);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::VCDIFF);

  CheckPatch(roboto_ab_, roboto_abcd_, response.Patch(), PatchFormat::VCDIFF);
}

TEST_F(PatchSubsetServerIntegrationTest, BadOriginalChecksum) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;

  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  request.SetProtocolVersion(ProtocolVersion::ONE);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetAcceptFormats({PatchFormat::BROTLI_SHARED_DICT});
  request.SetOriginalFontChecksum(0);
  request.SetBaseChecksum(subset_ab_checksum);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(empty_, roboto_abcd_, response.Replacement());
}

TEST_F(PatchSubsetServerIntegrationTest, BadBaseChecksum) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetAcceptFormats({PatchFormat::BROTLI_SHARED_DICT});
  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(0);
  request.SetProtocolVersion(ProtocolVersion::ONE);

  PatchResponse response;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, response),
            absl::OkStatus());

  EXPECT_EQ(response.OriginalFontChecksum(), original_font_checksum);
  EXPECT_EQ(response.PatchedChecksum(), subset_abcd_checksum);
  EXPECT_EQ(response.GetPatchFormat(), PatchFormat::BROTLI_SHARED_DICT);

  CheckPatch(empty_, roboto_abcd_, response.Replacement());
}

}  // namespace patch_subset
