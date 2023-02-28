#include "gtest/gtest.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
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
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;

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
    EXPECT_TRUE(font_provider_->GetFont("Roboto-Regular.ttf", &roboto_).ok());
    EXPECT_TRUE(
        font_provider_->GetFont("Roboto[wdth,wght].ttf", &roboto_variable_)
            .ok());

    original_font_checksum = hasher_.Checksum(roboto_.str());
    variable_original_font_checksum = hasher_.Checksum(roboto_variable_.str());
  }

  FontData CheckPatch(const FontData& base, const FontData& target,
                      string_view patch_string,
                      std::string encoding = Encodings::kBrotliDiffEncoding) {
    std::unique_ptr<BinaryDiff> binary_diff;
    std::unique_ptr<BinaryPatch> binary_patch;
    if (encoding == Encodings::kBrotliDiffEncoding) {
      binary_diff = std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff());
      binary_patch = std::unique_ptr<BinaryPatch>(new BrotliBinaryPatch());
    } else if (encoding == Encodings::kVCDIFFEncoding) {
      binary_diff = std::unique_ptr<BinaryDiff>(new VCDIFFBinaryDiff());
      binary_patch = std::unique_ptr<BinaryPatch>(new VCDIFFBinaryPatch());
    }

    FontData actual_target;
    FontData patch;
    patch.copy(patch_string.data(), patch_string.size());
    EXPECT_EQ(binary_patch->Patch(base, patch, &actual_target),
              absl::OkStatus());
    EXPECT_EQ(actual_target.str(), target.str());

    return actual_target;
  }

  absl::StatusOr<FontData> MakeSubset(const hb_set_t& codepoints,
                                      const ClientState& client_state,
                                      const FontData* base = nullptr) {
    std::string client_state_table;
    Status sc = client_state.SerializeToString(client_state_table);
    if (!sc.ok()) {
      return sc;
    }

    if (!base) {
      base = &roboto_;
    }

    FontData subset;
    sc = subsetter_.Subset(roboto_, codepoints, client_state_table, &subset);
    if (!sc.ok()) {
      return sc;
    }

    return subset;
  }

  absl::StatusOr<ClientState> GetStateTable(const FontData& font) {
    hb_face_t* face = font.reference_face();
    auto state = ClientState::FromFont(face);
    hb_face_destroy(face);
    return state;
  }

  FontProvider* font_provider_;
  PatchSubsetServerImpl server_;
  HarfbuzzSubsetter subsetter_;
  FastHasher hasher_;

  FontData empty_;
  FontData roboto_;
  FontData roboto_variable_;

  uint64_t original_font_checksum;
  uint64_t variable_original_font_checksum;
};

TEST_F(PatchSubsetServerIntegrationTest, NewRequest) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  FontData response;
  std::string encoding;
  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  ASSERT_EQ(encoding, Encodings::kBrotliDiffEncoding);
  response = CheckPatch(empty_, *expected, response.str());

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, NewRequest_Variable) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(variable_original_font_checksum);

  AxisSpace expected_space;
  expected_space.AddInterval(HB_TAG('w', 'g', 'h', 't'),
                             AxisInterval(100, 900));
  expected_space.AddInterval(HB_TAG('w', 'd', 't', 'h'), AxisInterval(75, 100));
  expected_state.SetSubsetAxisSpace(expected_space);
  expected_state.SetOriginalAxisSpace(expected_space);

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  FontData response;
  std::string encoding;
  ASSERT_EQ(
      server_.Handle("Roboto[wdth,wght].ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  BrotliBinaryPatch patcher;
  FontData subset;
  EXPECT_EQ(patcher.Patch(empty_, response, &subset), absl::OkStatus());

  auto state = GetStateTable(subset);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, NewRequestVCDIFF) {
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  FontData response;
  std::string encoding;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", {Encodings::kVCDIFFEncoding},
                           request, response, encoding),
            absl::OkStatus());

  EXPECT_EQ(encoding, Encodings::kVCDIFFEncoding);
  response =
      CheckPatch(empty_, *expected, response.str(), Encodings::kVCDIFFEncoding);

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, PatchRequest) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto base = MakeSubset(*set_ab, expected_state);
  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();
  ASSERT_TRUE(base.ok()) << base.status();

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(hasher_.Checksum(base->str()));

  FontData response;
  std::string encoding;
  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
  response = CheckPatch(*base, *expected, response.str());

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, PatchRequestVCDIFF) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto base = MakeSubset(*set_ab, expected_state);
  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();
  ASSERT_TRUE(base.ok()) << base.status();

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(hasher_.Checksum(base->str()));

  FontData response;
  std::string encoding;
  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", {Encodings::kVCDIFFEncoding},
                           request, response, encoding),
            absl::OkStatus());

  EXPECT_EQ(encoding, Encodings::kVCDIFFEncoding);
  response =
      CheckPatch(*base, *expected, response.str(), Encodings::kVCDIFFEncoding);

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();

  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, BadOriginalChecksum) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto base = MakeSubset(*set_ab, expected_state);
  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();
  ASSERT_TRUE(base.ok()) << base.status();

  PatchRequest request;

  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);

  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  request.SetOriginalFontChecksum(0);
  request.SetBaseChecksum(hasher_.Checksum(base->str()));

  FontData response;
  std::string encoding;
  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
  CheckPatch(empty_, *expected,
             response.str());  // Verify this is a replacement
  response = CheckPatch(*base, *expected, response.str());

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

TEST_F(PatchSubsetServerIntegrationTest, BadBaseChecksum) {
  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  ClientState expected_state;
  expected_state.SetOriginalFontChecksum(original_font_checksum);

  auto base = MakeSubset(*set_ab, expected_state);
  auto expected = MakeSubset(*set_abcd, expected_state);
  ASSERT_TRUE(expected.ok()) << expected.status();
  ASSERT_TRUE(base.ok()) << base.status();

  PatchRequest request;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(original_font_checksum);
  request.SetBaseChecksum(0);

  FontData response;
  std::string encoding;
  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
  CheckPatch(empty_, *expected,
             response.str());  // Verify this is a replacement
  response = CheckPatch(*base, *expected, response.str());

  auto state = GetStateTable(response);
  ASSERT_TRUE(state.ok()) << state.status();
  EXPECT_EQ(*state, expected_state);
}

}  // namespace patch_subset
