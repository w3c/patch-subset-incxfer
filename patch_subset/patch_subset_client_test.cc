#include "patch_subset/patch_subset_client.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "patch_subset/codepoint_map.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/file_font_provider.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/mock_binary_patch.h"
#include "patch_subset/mock_hasher.h"
#include "patch_subset/mock_integer_list_checksum.h"
#include "patch_subset/mock_patch_subset_server.h"
#include "patch_subset/null_request_logger.h"

namespace patch_subset {

using absl::Status;
using absl::StatusOr;
using absl::string_view;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Invoke;
using testing::Return;

static uint64_t kOriginalChecksum = 1;
static uint64_t kBaseChecksum = 2;
static uint64_t kPatchedChecksum = 3;

class PatchSubsetClientTest : public ::testing::Test {
 protected:
  PatchSubsetClientTest()
      : binary_patch_(new MockBinaryPatch()),
        hasher_(new MockHasher()),
        integer_list_checksum_(new MockIntegerListChecksum()),
        client_(new PatchSubsetClient(
            std::unique_ptr<BinaryPatch>(binary_patch_),
            std::unique_ptr<Hasher>(hasher_),
            std::unique_ptr<IntegerListChecksum>(integer_list_checksum_))),
        font_provider_(new FileFontProvider("patch_subset/testdata/")) {
    EXPECT_TRUE(
        font_provider_->GetFont("Roboto-Regular.ab.ttf", &roboto_ab_).ok());
  }

  StatusOr<FontData> AddStateToSubset(const FontData& font,
                                      const ClientState& state) {
    hb_face_t* face = font.reference_face();

    hb_subset_input_t* input = hb_subset_input_create_or_fail();
    if (!input) {
      return absl::InternalError("Failed to create subset input.");
    }
    hb_subset_input_keep_everything(input);

    hb_face_t* subset = hb_subset_or_fail(face, input);
    hb_face_destroy(face);
    hb_subset_input_destroy(input);

    if (!subset) {
      return absl::InternalError("Subsetting operation failed.");
    }

    std::string state_raw;
    Status sc = state.SerializeToString(state_raw);
    if (!sc.ok()) {
      hb_face_destroy(subset);
      return sc;
    }

    hb_blob_t* state_blob =
        hb_blob_create(state_raw.data(), state_raw.size(),
                       HB_MEMORY_MODE_READONLY, nullptr, nullptr);

    if (!hb_face_builder_add_table(subset, HB_TAG('I', 'F', 'T', 'P'),
                                   state_blob)) {
      hb_face_destroy(subset);
      hb_blob_destroy(state_blob);
      return absl::InternalError("Adding IFTP table to face failed.");
    }
    hb_blob_destroy(state_blob);

    hb_blob_t* subset_blob = hb_face_reference_blob(subset);
    FontData subset_data(subset_blob);

    hb_face_destroy(subset);
    hb_blob_destroy(subset_blob);

    return subset_data;
  }

  PatchRequest CreateRequest(const hb_set_t& codepoints) {
    PatchRequest request;
    patch_subset::cbor::CompressedSet codepoints_needed;
    CompressedSet::Encode(codepoints, codepoints_needed);
    request.SetCodepointsNeeded(codepoints_needed);
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

    request.SetOriginalFontChecksum(kOriginalChecksum);
    request.SetBaseChecksum(kBaseChecksum);
    return request;
  }

  void ExpectChecksum(string_view data, uint64_t checksum) {
    EXPECT_CALL(*hasher_, Checksum(data)).WillRepeatedly(Return(checksum));
  }

  void ExpectChecksum(const std::vector<int32_t>& data, uint64_t checksum) {
    EXPECT_CALL(*integer_list_checksum_, Checksum(data))
        .WillRepeatedly(Return(checksum));
  }

  MockBinaryPatch* binary_patch_;
  MockHasher* hasher_;
  MockIntegerListChecksum* integer_list_checksum_;

  std::unique_ptr<PatchSubsetClient> client_;

  std::unique_ptr<FontProvider> font_provider_;
  FontData roboto_ab_;
};

TEST_F(PatchSubsetClientTest, CreateNewRequest) {
  hb_set_unique_ptr codepoints = make_hb_set_from_ranges(1, 0x61, 0x64);

  FontData empty;
  auto request = client_->CreateRequest(*codepoints, empty);
  ASSERT_TRUE(request.ok()) << request.status();

  PatchRequest expected_request = CreateRequest(*codepoints);
  EXPECT_EQ(expected_request, *request);
}

TEST_F(PatchSubsetClientTest, SendPatchRequest) {
  hb_set_unique_ptr codepoints_have = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x63, 0x64);
  PatchRequest expected_request =
      CreateRequest(*codepoints_have, *codepoints_needed);

  ClientState state;
  state.SetOriginalFontChecksum(kOriginalChecksum);

  auto subset = AddStateToSubset(roboto_ab_, state);
  ASSERT_TRUE(subset.ok()) << subset.status();

  ExpectChecksum(subset->str(), kBaseChecksum);

  auto request = client_->CreateRequest(*codepoints_needed, *subset);
  ASSERT_TRUE(request.ok()) << request.status();

  EXPECT_EQ(expected_request, *request);
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

  CodepointMap map;
  map.AddMapping(0x61, 0);
  map.AddMapping(0x62, 1);
  map.AddMapping(0x63, 2);
  map.AddMapping(0x64, 3);

  ClientState state;
  state.SetOriginalFontChecksum(kOriginalChecksum);
  std::vector<int32_t> remapping;
  ASSERT_TRUE(map.ToVector(&remapping).ok());
  state.SetCodepointOrdering(remapping);

  auto subset = AddStateToSubset(roboto_ab_, state);
  ASSERT_TRUE(subset.ok()) << subset.status();

  ExpectChecksum(subset->str(), kBaseChecksum);
  ExpectChecksum(remapping, 13);

  auto request = client_->CreateRequest(*codepoints_needed, *subset);
  ASSERT_TRUE(request.ok()) << request.status();

  EXPECT_EQ(expected_request, *request);
}

TEST_F(PatchSubsetClientTest, SendPatchRequest_RemovesExistingCodepoints) {
  hb_set_unique_ptr codepoints_have = make_hb_set_from_ranges(1, 0x61, 0x62);
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x63, 0x64);
  PatchRequest expected_request =
      CreateRequest(*codepoints_have, *codepoints_needed);

  hb_set_union(codepoints_needed.get(), codepoints_have.get());
  ClientState state;
  state.SetOriginalFontChecksum(kOriginalChecksum);

  auto subset = AddStateToSubset(roboto_ab_, state);
  ASSERT_TRUE(subset.ok()) << subset.status();

  ExpectChecksum(subset->str(), kBaseChecksum);

  auto request = client_->CreateRequest(*codepoints_needed, *subset);
  ASSERT_TRUE(request.ok()) << request.status();
  EXPECT_EQ(expected_request, *request);
}

TEST_F(PatchSubsetClientTest, DoesntSendPatchRequest_NoNewCodepoints) {
  hb_set_unique_ptr codepoints_needed = make_hb_set_from_ranges(1, 0x61, 0x62);

  ClientState state;
  state.SetOriginalFontChecksum(kOriginalChecksum);

  auto subset = AddStateToSubset(roboto_ab_, state);
  ASSERT_TRUE(subset.ok()) << subset.status();

  auto request = client_->CreateRequest(*codepoints_needed, *subset);
  ASSERT_TRUE(request.ok()) << request.status();

  PatchRequest expected_request;
  EXPECT_EQ(expected_request, *request);
}

// TODO(garretrieger): add tests for DecodeResponse.
// TODO(garretrieger): convert below to tests of DecodeResponse.

/*

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
  EXPECT_EQ(client_->Extend(*codepoints, state), absl::OkStatus());

  EXPECT_EQ(state.FontData(), "roboto.patched.ttf");
  EXPECT_EQ(state.OriginalFontChecksum(), kOriginalChecksum);
}

TEST_F(PatchSubsetClientTest, HandlesRebaseResponse_WithCodepointMapping) {
  hb_set_unique_ptr codepoints = make_hb_set(1, 0x61);

  PatchResponse response = CreateResponse(false);  // Rebase.
  response.SetCodepointOrdering(std::vector<int32_t>{13});
  response.SetOrderingChecksum(14);

  SendResponse(response);
  ExpectChecksum("roboto.patched.ttf", kPatchedChecksum);

  FontData base("");
  FontData patch("roboto.patch.ttf");
  ExpectPatch(base, patch, "roboto.patched.ttf");

  ClientState state;
  state.SetFontId("roboto.base.ttf");
  EXPECT_EQ(client_->Extend(*codepoints, state), absl::OkStatus());

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
  EXPECT_EQ(client_->Extend(*codepoints, state), absl::OkStatus());

  EXPECT_EQ(state.FontData(), "roboto.patched.ttf");
  EXPECT_EQ(state.OriginalFontChecksum(), kOriginalChecksum);
}
    */

// TODO(garretrieger): add more response handling tests:
//   - checksum mismatch.
//   - bad patch format.

}  // namespace patch_subset
