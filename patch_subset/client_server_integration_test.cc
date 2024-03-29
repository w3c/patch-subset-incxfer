#include "common/brotli_binary_diff.h"
#include "common/brotli_binary_patch.h"
#include "common/fast_hasher.h"
#include "common/file_font_provider.h"
#include "common/font_data.h"
#include "common/hasher.h"
#include "common/hb_set_unique_ptr.h"
#include "gtest/gtest.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/harfbuzz_subsetter.h"
#include "patch_subset/integer_list_checksum.h"
#include "patch_subset/integer_list_checksum_impl.h"
#include "patch_subset/noop_codepoint_predictor.h"
#include "patch_subset/null_request_logger.h"
#include "patch_subset/patch_subset_client.h"
#include "patch_subset/patch_subset_server_impl.h"
#include "patch_subset/simple_codepoint_mapper.h"
#include "patch_subset/simulation.h"
#include "patch_subset/vcdiff_binary_diff.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using common::BinaryDiff;
using common::BinaryPatch;
using common::BrotliBinaryDiff;
using common::BrotliBinaryPatch;
using common::FastHasher;
using common::FileFontProvider;
using common::FontData;
using common::FontProvider;
using common::Hasher;
using common::hb_set_unique_ptr;
using common::make_hb_set_from_ranges;
using patch_subset::cbor::ClientState;

class PatchSubsetClientServerIntegrationTest : public ::testing::Test {
 protected:
  const std::string kTestDataDir = "patch_subset/testdata/";

  PatchSubsetClientServerIntegrationTest()
      : hasher_(new FastHasher()),

        server_(
            0,
            std::unique_ptr<FontProvider>(new FileFontProvider(kTestDataDir)),
            std::unique_ptr<Subsetter>(new HarfbuzzSubsetter()),
            std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff()),
            std::unique_ptr<BinaryDiff>(new VCDIFFBinaryDiff()),
            std::unique_ptr<Hasher>(new FastHasher()),
            std::unique_ptr<CodepointMapper>(nullptr),
            std::unique_ptr<IntegerListChecksum>(nullptr),
            std::unique_ptr<CodepointPredictor>(new NoopCodepointPredictor())),

        client_(std::unique_ptr<BinaryPatch>(new BrotliBinaryPatch()),
                std::unique_ptr<Hasher>(new FastHasher()),
                std::unique_ptr<IntegerListChecksum>(
                    new IntegerListChecksumImpl(hasher_.get()))),

        server_with_mapping_(
            0,
            std::unique_ptr<FontProvider>(new FileFontProvider(kTestDataDir)),
            std::unique_ptr<Subsetter>(new HarfbuzzSubsetter()),
            std::unique_ptr<BinaryDiff>(new BrotliBinaryDiff()),
            std::unique_ptr<BinaryDiff>(new VCDIFFBinaryDiff()),
            std::unique_ptr<Hasher>(new FastHasher()),
            std::unique_ptr<CodepointMapper>(new SimpleCodepointMapper()),
            std::unique_ptr<IntegerListChecksum>(
                new IntegerListChecksumImpl(hasher_.get())),
            std::unique_ptr<CodepointPredictor>(new NoopCodepointPredictor())) {
    FileFontProvider font_provider(kTestDataDir);
    Status s = font_provider.GetFont("Roboto-Regular.ttf", &roboto_);
    assert(s.ok());

    hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
    hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

    state_.SetOriginalFontChecksum(0xC722EE0E33D3B460);

    std::string state_string;
    s.Update(state_.SerializeToString(state_string));
    s.Update(subsetter_.Subset(roboto_, *set_ab, state_string, &roboto_ab_));
    s.Update(
        subsetter_.Subset(roboto_, *set_abcd, state_string, &roboto_abcd_));

    assert(s.ok());
  }

  absl::StatusOr<ClientState> GetStateTable(const FontData& font) {
    hb_face_t* face = font.reference_face();
    auto state = ClientState::FromFont(face);
    hb_face_destroy(face);
    return state;
  }

  std::unique_ptr<Hasher> hasher_;
  NullRequestLogger request_logger_;

  PatchSubsetServerImpl server_;
  PatchSubsetClient client_;

  PatchSubsetServerImpl server_with_mapping_;

  HarfbuzzSubsetter subsetter_;

  ClientState state_;
  FontData empty_;
  FontData roboto_;
  FontData roboto_abcd_;
  FontData roboto_ab_;
};

TEST_F(PatchSubsetClientServerIntegrationTest, Session) {
  Simulation simulation(&client_, &server_, &request_logger_);

  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);

  auto result = simulation.Extend("Roboto-Regular.ttf", *set_ab, empty_);
  ASSERT_TRUE(result.ok()) << result.status();

  auto state = GetStateTable(*result);
  ASSERT_TRUE(state.ok()) << state.status();

  EXPECT_EQ(state->OriginalFontChecksum(), 0xC722EE0E33D3B460);
  EXPECT_EQ(result->str(), roboto_ab_.str());

  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);
  result = simulation.Extend("Roboto-Regular.ttf", *set_abcd, *result);
  ASSERT_TRUE(result.ok()) << result.status();

  state = GetStateTable(*result);
  ASSERT_TRUE(state.ok()) << state.status();

  EXPECT_EQ(state->OriginalFontChecksum(), 0xC722EE0E33D3B460);
  EXPECT_EQ(result->str(), roboto_abcd_.string());
  EXPECT_TRUE(state->CodepointOrdering().empty());
}

TEST_F(PatchSubsetClientServerIntegrationTest, SessionWithCodepointOrdering) {
  Simulation simulation(&client_, &server_with_mapping_, &request_logger_);
  IntegerListChecksumImpl checksummer(hasher_.get());

  hb_set_unique_ptr set_ab = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr set_abcd = make_hb_set_from_ranges(1, 0x61, 0x64);

  auto result = simulation.Extend("Roboto-Regular.ttf", *set_ab, empty_);
  ASSERT_TRUE(result.ok()) << result.status();

  auto state = GetStateTable(*result);
  ASSERT_TRUE(state.ok()) << state.status();

  Status s;
  std::string state_string;
  s.Update(state->SerializeToString(state_string));
  s.Update(subsetter_.Subset(roboto_, *set_ab, state_string, &roboto_ab_));
  s.Update(subsetter_.Subset(roboto_, *set_abcd, state_string, &roboto_abcd_));
  ASSERT_EQ(s, absl::OkStatus());

  EXPECT_EQ(state->OriginalFontChecksum(), 0xC722EE0E33D3B460);
  EXPECT_EQ(result->str(), roboto_ab_.string());
  EXPECT_FALSE(state->CodepointOrdering().empty());
  EXPECT_EQ(checksummer.Checksum(state->CodepointOrdering()),
            0xD5BD080511DD60DD);

  result = simulation.Extend("Roboto-Regular.ttf", *set_abcd, *result);
  ASSERT_TRUE(result.ok()) << result.status();

  state = GetStateTable(*result);
  ASSERT_TRUE(state.ok()) << state.status();

  EXPECT_EQ(state->OriginalFontChecksum(), 0xC722EE0E33D3B460);
  EXPECT_EQ(result->str(), roboto_abcd_.string());
  EXPECT_FALSE(state->CodepointOrdering().empty());
  EXPECT_EQ(checksummer.Checksum(state->CodepointOrdering()),
            0xD5BD080511DD60DD);
}

}  // namespace patch_subset
