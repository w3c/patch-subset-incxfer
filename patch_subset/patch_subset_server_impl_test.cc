#include "patch_subset/patch_subset_server_impl.h"

#include <algorithm>

#include "absl/strings/string_view.h"
#include "common/hb_set_unique_ptr.h"
#include "common/mock_binary_diff.h"
#include "common/mock_font_provider.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/encodings.h"
#include "patch_subset/fake_subsetter.h"
#include "patch_subset/mock_codepoint_predictor.h"
#include "patch_subset/mock_hasher.h"
#include "patch_subset/mock_integer_list_checksum.h"
#include "patch_subset/simple_codepoint_mapper.h"

namespace patch_subset {

using absl::Status;
using absl::string_view;
using common::BinaryDiff;
using common::FontData;
using common::FontProvider;
using common::Hasher;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::make_hb_set_from_ranges;
using common::MockBinaryDiff;
using common::MockFontProvider;
using patch_subset::MockIntegerListChecksum;
using patch_subset::cbor::ClientState;
using patch_subset::cbor::PatchRequest;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Return;

MATCHER_P(EqualsSet, other, "") { return hb_set_is_equal(arg, other); }

Status returnFontId(const std::string& id, FontData* out) {
  out->copy(id.c_str(), id.size());
  return absl::OkStatus();
}

Status diff(const FontData& font_base, const FontData& font_derived,
            FontData* out /* OUT */) {
  if (font_base.empty()) {
    out->copy(font_derived.data(), font_derived.size());
    return absl::OkStatus();
  }

  std::string base(font_base.data(), font_base.size());
  std::string derived(font_derived.data(), font_derived.size());
  std::string patch(derived + " - " + base);
  out->copy(patch.c_str(), patch.size());
  return absl::OkStatus();
}

class PatchSubsetServerImplTestBase : public ::testing::Test {
 protected:
  PatchSubsetServerImplTestBase()
      : font_provider_(new MockFontProvider()),
        brotli_binary_diff_(new MockBinaryDiff()),
        vcdiff_binary_diff_(new MockBinaryDiff()),
        hasher_(new MockHasher()),
        codepoint_predictor_(new MockCodepointPredictor()),
        set_abcd_(make_hb_set_from_ranges(1, 0x61, 0x64)),
        set_ab_(make_hb_set_from_ranges(1, 0x61, 0x62)) {}

  void ExpectBrotliDiff() {
    EXPECT_CALL(*brotli_binary_diff_, Diff(_, _, _))
        .Times(1)
        .WillRepeatedly(Invoke(diff));
  }

  void ExpectVCDIFF() {
    EXPECT_CALL(*vcdiff_binary_diff_, Diff(_, _, _))
        .Times(1)
        .WillRepeatedly(Invoke(diff));
  }

  void ExpectRoboto() {
    EXPECT_CALL(*font_provider_, GetFont("Roboto-Regular.ttf", _))
        .Times(1)
        .WillRepeatedly(Invoke(returnFontId));
  }

  void ExpectChecksum(string_view value, uint64_t checksum) {
    EXPECT_CALL(*hasher_, Checksum(value)).WillRepeatedly(Return(checksum));
  }

  void AddPredictedCodepoints(const hb_set_t* font_codepoints,
                              const hb_set_t* have_codepoints,
                              const hb_set_t* requested_codepoints,
                              const hb_set_t* codepoints_to_add) {
    EXPECT_CALL(*codepoint_predictor_,
                Predict(EqualsSet(font_codepoints), EqualsSet(have_codepoints),
                        EqualsSet(requested_codepoints), 50, _))
        .Times(1)
        .WillRepeatedly(Invoke(AddCodepoints(codepoints_to_add)));
  }

  MockFontProvider* font_provider_;
  MockBinaryDiff* brotli_binary_diff_;
  MockBinaryDiff* vcdiff_binary_diff_;
  MockHasher* hasher_;
  MockCodepointPredictor* codepoint_predictor_;
  hb_set_unique_ptr set_abcd_;
  hb_set_unique_ptr set_ab_;
};

class PatchSubsetServerImplTest : public PatchSubsetServerImplTestBase {
 protected:
  PatchSubsetServerImplTest()
      : server_(50, std::unique_ptr<FontProvider>(font_provider_),
                std::unique_ptr<Subsetter>(new FakeSubsetter()),
                std::unique_ptr<BinaryDiff>(brotli_binary_diff_),
                std::unique_ptr<BinaryDiff>(vcdiff_binary_diff_),
                std::unique_ptr<Hasher>(hasher_),
                std::unique_ptr<CodepointMapper>(nullptr),
                std::unique_ptr<IntegerListChecksum>(nullptr),
                std::unique_ptr<CodepointPredictor>(codepoint_predictor_)) {}

  PatchSubsetServerImpl server_;
};

class PatchSubsetServerImplWithCodepointRemappingTest
    : public PatchSubsetServerImplTestBase {
 protected:
  PatchSubsetServerImplWithCodepointRemappingTest()
      : integer_list_checksum_(new MockIntegerListChecksum()),
        server_(50, std::unique_ptr<FontProvider>(font_provider_),
                std::unique_ptr<Subsetter>(new FakeSubsetter()),
                std::unique_ptr<BinaryDiff>(brotli_binary_diff_),
                std::unique_ptr<BinaryDiff>(vcdiff_binary_diff_),
                std::unique_ptr<Hasher>(hasher_),
                std::unique_ptr<CodepointMapper>(new SimpleCodepointMapper()),
                std::unique_ptr<IntegerListChecksum>(integer_list_checksum_),
                std::unique_ptr<CodepointPredictor>(codepoint_predictor_)),
        set_abcd_encoded_(make_hb_set_from_ranges(1, 0, 3)),
        set_ab_encoded_(make_hb_set_from_ranges(1, 0, 1)) {}

  void ExpectCodepointMappingChecksum(std::vector<int> mapping_deltas,
                                      uint64_t checksum) {
    std::vector<int32_t> compressed_list;
    for (int delta : mapping_deltas) {
      compressed_list.push_back(delta);
    }

    EXPECT_CALL(*integer_list_checksum_, Checksum(Eq(compressed_list)))
        .WillRepeatedly(Return(checksum));
  }

  MockIntegerListChecksum* integer_list_checksum_;
  PatchSubsetServerImpl server_;

  hb_set_unique_ptr set_abcd_encoded_;
  hb_set_unique_ptr set_ab_encoded_;
};

absl::StatusOr<ClientState> GetStateTable(const FontData& font) {
  hb_face_t* face = font.reference_face();
  auto state = ClientState::FromFont(face);
  hb_face_destroy(face);
  return state;
}

// TODO(garretrieger): subsetter failure test.

TEST_F(PatchSubsetServerImplTest, NewRequest) {
  ExpectRoboto();
  ExpectBrotliDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplTest, NewRequest_NoSharedBrotli) {
  ExpectRoboto();
  ExpectBrotliDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliEncoding},
                           request, response, encoding),
            absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliEncoding);
}

TEST_F(PatchSubsetServerImplTest, NewRequestVCDIFF) {
  ExpectRoboto();
  ExpectVCDIFF();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", {Encodings::kVCDIFFEncoding},
                           request, response, encoding),
            absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kVCDIFFEncoding);
}

TEST_F(PatchSubsetServerImplTest, PrefersBrotli) {
  ExpectRoboto();
  ExpectBrotliDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_EQ(server_.Handle(
                "Roboto-Regular.ttf",
                {Encodings::kBrotliDiffEncoding, Encodings::kVCDIFFEncoding},
                request, response, encoding),
            absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest,
       NewRequestWithCodepointRemapping) {
  ExpectRoboto();
  ExpectBrotliDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  ExpectCodepointMappingChecksum({97, 98, 99, 100, 101, 102}, 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(
      response.str(),
      "Roboto-Regular.ttf:abcd, {orig_cs=42,cp_rm=[97,98,99,100,101,102]}");
}

TEST_F(PatchSubsetServerImplTest, PatchRequest) {
  ExpectRoboto();
  ExpectBrotliDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab, {orig_cs=42}", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcd, {orig_cs=42}", 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(42);
  request.SetBaseChecksum(43);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(response.str(),
            "Roboto-Regular.ttf:abcd, {orig_cs=42} - Roboto-Regular.ttf:ab, "
            "{orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplTest, PatchRequestWithCodepointPrediction) {
  ExpectRoboto();
  ExpectBrotliDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab, {orig_cs=42}", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcde, {orig_cs=42}", 44);

  hb_set_unique_ptr font_codepoints = make_hb_set_from_ranges(1, 0x61, 0x66);
  hb_set_unique_ptr have_codepoints = make_hb_set(2, 0x61, 0x62);
  hb_set_unique_ptr requested_codepoints = make_hb_set(2, 0x63, 0x64);
  hb_set_unique_ptr codepoints_to_add = make_hb_set(1, 'e');
  AddPredictedCodepoints(font_codepoints.get(), have_codepoints.get(),
                         requested_codepoints.get(), codepoints_to_add.get());

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(42);
  request.SetBaseChecksum(43);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(response.str(),
            "Roboto-Regular.ttf:abcde, {orig_cs=42} - Roboto-Regular.ttf:ab, "
            "{orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest,
       PatchRequestWithCodepointRemapping) {
  ExpectRoboto();
  ExpectBrotliDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum(
      "Roboto-Regular.ttf:ab, {orig_cs=42,cp_rm=[97,98,99,100,101,102]}", 43);
  ExpectChecksum(
      "Roboto-Regular.ttf:abcd, {orig_cs=42,cp_rm=[97,98,99,100,101,102]}", 44);
  ExpectCodepointMappingChecksum({97, 98, 99, 100, 101, 102}, 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_encoded_, codepoints_have);
  request.SetIndicesHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_encoded_, codepoints_needed);
  request.SetIndicesNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(42);
  request.SetBaseChecksum(43);
  request.SetOrderingChecksum(44);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(
      response.str(),
      "Roboto-Regular.ttf:abcd, {orig_cs=42,cp_rm=[97,98,99,100,101,102]} - "
      "Roboto-Regular.ttf:ab, {orig_cs=42,cp_rm=[97,98,99,100,101,102]}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest, BadIndexChecksum) {
  ExpectRoboto();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectCodepointMappingChecksum({97, 98, 99, 100, 101, 102}, 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_, codepoints_have);
  request.SetIndicesHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetIndicesNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(42);
  request.SetBaseChecksum(43);
  request.SetOrderingChecksum(123);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  // If the codepoint ordering is not understood, then the server will respond
  // with an unsubsetted font.
  EXPECT_EQ(response.str(), "Roboto-Regular.ttf");
}

TEST_F(PatchSubsetServerImplTest, BadOriginalFontChecksum) {
  ExpectRoboto();
  ExpectBrotliDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd, {orig_cs=42}", 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(100);
  request.SetBaseChecksum(43);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplTest, BadBaseChecksum) {
  ExpectRoboto();
  ExpectBrotliDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab, {orig_cs=42}", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcd, {orig_cs=42}", 44);

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_have;
  CompressedSet::Encode(*set_ab_, codepoints_have);
  request.SetCodepointsHave(codepoints_have);
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetOriginalFontChecksum(42);
  request.SetBaseChecksum(100);

  EXPECT_EQ(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding),
      absl::OkStatus());

  EXPECT_EQ(response.str(), "Roboto-Regular.ttf:abcd, {orig_cs=42}");
  EXPECT_EQ(encoding, Encodings::kBrotliDiffEncoding);
}

TEST_F(PatchSubsetServerImplTest, NotFound) {
  EXPECT_CALL(*font_provider_, GetFont("Roboto-Regular.ttf", _))
      .Times(1)
      .WillRepeatedly(Return(absl::NotFoundError("not found.")));

  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);

  EXPECT_TRUE(absl::IsNotFound(server_.Handle("Roboto-Regular.ttf",
                                              {Encodings::kBrotliDiffEncoding},
                                              request, response, encoding)));
}

TEST_F(PatchSubsetServerImplTest, RejectsMissingBaseChecksum) {
  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetCodepointsNeeded(codepoints_needed);
  request.SetCodepointsHave(codepoints_needed);

  // base checksum and original font checksum are missing.
  EXPECT_TRUE(absl::IsInvalidArgument(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding)));
}

TEST_F(PatchSubsetServerImplTest, RejectsMissingOrderingChecksum) {
  PatchRequest request;
  FontData response;
  std::string encoding;
  patch_subset::cbor::CompressedSet codepoints_needed;
  CompressedSet::Encode(*set_abcd_, codepoints_needed);
  request.SetIndicesNeeded(codepoints_needed);

  // ordering checksum is missing.
  EXPECT_TRUE(absl::IsInvalidArgument(
      server_.Handle("Roboto-Regular.ttf", {Encodings::kBrotliDiffEncoding},
                     request, response, encoding)));
}

}  // namespace patch_subset
