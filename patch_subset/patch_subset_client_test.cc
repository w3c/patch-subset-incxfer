#include "patch_subset/patch_subset_client.h"

#include "common/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/mock_binary_patch.h"
#include "patch_subset/mock_hasher.h"
#include "patch_subset/mock_patch_subset_server.h"
#include "patch_subset/null_request_logger.h"

using ::absl::string_view;

using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using patch_subset::cbor::PatchResponse;
using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

namespace patch_subset {

static uint64_t kOriginalChecksum = 1;
static uint64_t kBaseChecksum = 2;
static uint64_t kPatchedChecksum = 3;

class PatchSubsetClientTest : public ::testing::Test {
 protected:
  PatchSubsetClientTest()
      : binary_patch_(new MockBinaryPatch()),
        hasher_(new MockHasher()),
        client_(
            new PatchSubsetClient(&server_, &request_logger_,
                                  std::unique_ptr<BinaryPatch>(binary_patch_),
                                  std::unique_ptr<Hasher>(hasher_))),
        font_provider_(new FileFontProvider("patch_subset/testdata/")) {
    font_provider_->GetFont("Roboto-Regular.ab.ttf", &roboto_ab_);
  }

  PatchRequest CreateRequest(const hb_set_t& codepoints) {
    PatchRequest request;
    patch_subset::cbor::CompressedSet codepoints_needed;
    CompressedSet::Encode(codepoints, codepoints_needed);
    request.SetCodepointsNeeded(codepoints_needed);
    request.AddAcceptFormat(PatchFormat::BROTLI_SHARED_DICT);
    return request;
  }

  PatchRequest CreateRequest(const hb_set_t& codepoints_have,
                             const hb_set_t& codepoints_needed) {
    PatchRequest request;
    if (!hb_set_is_empty(&codepoints_have)) {
      patch_subset::cbor::CompressedSet codepoints_have2;
      CompressedSet::Encode(codepoints_have, codepoints_have2);
      request.SetCodepointsHave(codepoints_have2);
    }

    if (!hb_set_is_empty(&codepoints_needed)) {
      patch_subset::cbor::CompressedSet codepoints_needed2;
      CompressedSet::Encode(codepoints_needed, codepoints_needed2);
      request.SetCodepointsNeeded(codepoints_needed2);
    }

    request.AddAcceptFormat(PatchFormat::BROTLI_SHARED_DICT);
    request.SetOriginalFontChecksum(kOriginalChecksum);
    request.SetBaseChecksum(kBaseChecksum);
    return request;
  }

  PatchResponse CreateResponse(bool patch) {
    PatchResponse response;
    response.SetOriginalFontChecksum(kOriginalChecksum);
    response.SetPatchFormat(PatchFormat::BROTLI_SHARED_DICT);
    if (patch) {
      response.SetPatch("roboto.patch.ttf");
    } else {
      response.SetReplacement("roboto.patch.ttf");
    }
    response.SetPatchedChecksum(kPatchedChecksum);
    return response;
  }

  void ExpectRequest(const PatchRequest& expected_request) {
    EXPECT_CALL(server_, Handle("roboto", Eq(expected_request), _))
        .Times(1)
        // Short circuit the response handling code.
        .WillOnce(Return(StatusCode::kInternal));
  }

  void ExpectChecksum(string_view data, uint64_t checksum) {
    EXPECT_CALL(*hasher_, Checksum(data)).WillRepeatedly(Return(checksum));
  }

  void SendResponse(const PatchResponse& response) {
    EXPECT_CALL(server_, Handle(_, _, _))
        .Times(1)
        .WillOnce(Invoke(ReturnResponse(response)));
  }

  void ExpectPatch(const FontData& base, const FontData& patch,
                   string_view patched) {
    EXPECT_CALL(*binary_patch_, Patch(Eq(ByRef(base)), Eq(ByRef(patch)), _))
        .Times(1)
        .WillOnce(Invoke(ApplyPatch(patched)));
  }

  MockPatchSubsetServer server_;
  NullRequestLogger request_logger_;
  MockBinaryPatch* binary_patch_;
  MockHasher* hasher_;

  std::unique_ptr<PatchSubsetClient> client_;

  std::unique_ptr<FontProvider> font_provider_;
  FontData roboto_ab_;
};

TEST_F(PatchSubsetClientTest, SendsNewRequest) {
  hb_set_unique_ptr codepoints = make_hb_set_from_ranges(1, 0x61, 0x64);
  PatchRequest expected_request = CreateRequest(*codepoints);
  ExpectRequest(expected_request);

  ClientState state;
  state.SetFontId("roboto");
  client_->Extend(*codepoints, state);
}

TEST_F(PatchSubsetClientTest, SendPatchRequest) {
  hb_set_unique_ptr codepoints_have = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x63, 0x64);
  PatchRequest expected_request =
      CreateRequest(*codepoints_have, *codepoints_needed);
  ExpectRequest(expected_request);
  ExpectChecksum(roboto_ab_.str(), kBaseChecksum);

  ClientState state;
  state.SetFontId("roboto");
  state.SetFontData(roboto_ab_.string());
  state.SetOriginalFontChecksum(kOriginalChecksum);
  client_->Extend(*codepoints_needed, state);
}

TEST_F(PatchSubsetClientTest, SendPatchRequest_WithCodepointMapping) {
  hb_set_unique_ptr empty_set = make_hb_set();
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x63, 0x65);
  hb_set_unique_ptr codepoints_have_encoded = make_hb_set_from_ranges(1, 0, 1);
  hb_set_unique_ptr codepoints_needed_encoded =
      make_hb_set_from_ranges(1, 2, 3);

  PatchRequest expected_request = CreateRequest(*empty_set, *empty_set);

  patch_subset::cbor::CompressedSet codepoints_have_compressed;
  CompressedSet::Encode(*codepoints_have_encoded, codepoints_have_compressed);
  expected_request.SetIndicesHave(codepoints_have_compressed);

  patch_subset::cbor::CompressedSet codepoints_needed_compressed;
  CompressedSet::Encode(*codepoints_needed_encoded,
                        codepoints_needed_compressed);
  expected_request.SetIndicesNeeded(codepoints_needed_compressed);

  expected_request.SetOrderingChecksum(13);

  ExpectRequest(expected_request);
  ExpectChecksum(roboto_ab_.str(), kBaseChecksum);

  CodepointMap map;
  map.AddMapping(0x61, 0);
  map.AddMapping(0x62, 1);
  map.AddMapping(0x63, 2);
  map.AddMapping(0x64, 3);

  ClientState state;
  state.SetFontId("roboto");
  state.SetFontData(roboto_ab_.string());
  state.SetOriginalFontChecksum(kOriginalChecksum);
  vector<int32_t> remapping;
  map.ToVector(&remapping);
  state.SetCodepointRemapping(remapping);
  state.SetCodepointRemappingChecksum(13);

  client_->Extend(*codepoints_needed, state);
}

TEST_F(PatchSubsetClientTest, SendPatchRequest_RemovesExistingCodepoints) {
  hb_set_unique_ptr codepoints_have = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x63, 0x64);
  PatchRequest expected_request =
      CreateRequest(*codepoints_have, *codepoints_needed);
  ExpectRequest(expected_request);
  ExpectChecksum(roboto_ab_.str(), kBaseChecksum);

  hb_set_union(codepoints_needed.get(), codepoints_have.get());
  ClientState state;
  state.SetFontId("roboto");
  state.SetFontData(roboto_ab_.string());
  state.SetOriginalFontChecksum(kOriginalChecksum);
  client_->Extend(*codepoints_needed, state);
}

TEST_F(PatchSubsetClientTest, DoesntSendPatchRequest_NoNewCodepoints) {
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x61, 0x62);

  EXPECT_CALL(server_, Handle(_, _, _)).Times(0);

  ClientState state;
  state.SetFontId("roboto");
  state.SetFontData(roboto_ab_.string());
  state.SetOriginalFontChecksum(kOriginalChecksum);
  EXPECT_EQ(client_->Extend(*codepoints_needed, state), StatusCode::kOk);
}

TEST_F(PatchSubsetClientTest, HandlesRebaseResponse) {
  hb_set_unique_ptr codepoints = make_hb_set(1, 0x61);

  PatchResponse response = CreateResponse(false);  // Rebase.
  SendResponse(response);
  ExpectChecksum("roboto.patched.ttf", kPatchedChecksum);

  FontData base("");
  FontData patch("roboto.patch.ttf");
  ExpectPatch(base, patch, "roboto.patched.ttf");

  ClientState state;
  state.SetFontData("roboto.base.ttf");
  EXPECT_EQ(client_->Extend(*codepoints, state), StatusCode::kOk);

  EXPECT_EQ(state.FontData(), "roboto.patched.ttf");
  EXPECT_EQ(state.OriginalFontChecksum(), kOriginalChecksum);
}

TEST_F(PatchSubsetClientTest, HandlesRebaseResponse_WithCodepointMapping) {
  hb_set_unique_ptr codepoints = make_hb_set(1, 0x61);

  PatchResponse response = CreateResponse(false);  // Rebase.
  response.SetCodepointOrdering(vector<int32_t>{13});
  response.SetOrderingChecksum(14);

  SendResponse(response);
  ExpectChecksum("roboto.patched.ttf", kPatchedChecksum);

  FontData base("");
  FontData patch("roboto.patch.ttf");
  ExpectPatch(base, patch, "roboto.patched.ttf");

  ClientState state;
  state.SetFontId("roboto.base.ttf");
  EXPECT_EQ(client_->Extend(*codepoints, state), StatusCode::kOk);

  EXPECT_EQ(state.FontData(), "roboto.patched.ttf");
  EXPECT_EQ(state.OriginalFontChecksum(), kOriginalChecksum);

  EXPECT_EQ(state.CodepointRemapping().size(), 1);
  EXPECT_EQ(state.CodepointRemapping()[0], 13);
  EXPECT_EQ(state.CodepointRemappingChecksum(), 14);
}

TEST_F(PatchSubsetClientTest, HandlesPatchResponse) {
  hb_set_unique_ptr codepoints = make_hb_set(1, 0x61);

  PatchResponse response = CreateResponse(true);  // Patch.

  SendResponse(response);
  ExpectChecksum("roboto.patched.ttf", kPatchedChecksum);

  FontData base("roboto.base.ttf");
  FontData patch("roboto.patch.ttf");
  ExpectPatch(base, patch, "roboto.patched.ttf");

  ClientState state;
  state.SetFontData("roboto.base.ttf");
  EXPECT_EQ(client_->Extend(*codepoints, state), StatusCode::kOk);

  EXPECT_EQ(state.FontData(), "roboto.patched.ttf");
  EXPECT_EQ(state.OriginalFontChecksum(), kOriginalChecksum);
}

// TODO(garretrieger): add more response handling tests:
//   - checksum mismatch.
//   - bad patch format.

}  // namespace patch_subset
