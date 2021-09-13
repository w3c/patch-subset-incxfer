#include "patch_subset/patch_subset_server_impl.h"

#include <google/protobuf/util/message_differencer.h>

#include <algorithm>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/codepoint_mapper.h"
#include "patch_subset/compressed_set.h"
#include "patch_subset/fake_subsetter.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/mock_binary_diff.h"
#include "patch_subset/mock_codepoint_predictor.h"
#include "patch_subset/mock_compressed_list_checksum.h"
#include "patch_subset/mock_font_provider.h"
#include "patch_subset/mock_hasher.h"
#include "patch_subset/simple_codepoint_mapper.h"

using ::absl::string_view;
using ::google::protobuf::util::MessageDifferencer;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace patch_subset {

MATCHER_P(EqualsProto, other, "") {
  return MessageDifferencer::Equals(arg, other);
}

MATCHER_P(EqualsSet, other, "") { return hb_set_is_equal(arg, other); }

StatusCode returnFontId(const std::string& id, FontData* out) {
  out->copy(id.c_str(), id.size());
  return StatusCode::kOk;
}

StatusCode diff(const FontData& font_base, const FontData& font_derived,
                FontData* out /* OUT */) {
  if (font_base.empty()) {
    out->copy(font_derived.data(), font_derived.size());
    return StatusCode::kOk;
  }

  std::string base(font_base.data(), font_base.size());
  std::string derived(font_derived.data(), font_derived.size());
  std::string patch(derived + " - " + base);
  out->copy(patch.c_str(), patch.size());
  return StatusCode::kOk;
}

class PatchSubsetServerImplTestBase : public ::testing::Test {
 protected:
  PatchSubsetServerImplTestBase()
      : font_provider_(new MockFontProvider()),
        binary_diff_(new MockBinaryDiff()),
        hasher_(new MockHasher()),
        codepoint_predictor_(new MockCodepointPredictor()),
        set_abcd_(make_hb_set_from_ranges(1, 0x61, 0x64)),
        set_ab_(make_hb_set_from_ranges(1, 0x61, 0x62)) {}

  void ExpectDiff() {
    EXPECT_CALL(*binary_diff_, Diff(_, _, _))
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
  MockBinaryDiff* binary_diff_;
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
                std::unique_ptr<BinaryDiff>(binary_diff_),
                std::unique_ptr<Hasher>(hasher_),
                std::unique_ptr<CodepointMapper>(nullptr),
                std::unique_ptr<CompressedListChecksum>(nullptr),
                std::unique_ptr<CodepointPredictor>(codepoint_predictor_)) {}

  PatchSubsetServerImpl server_;
};

class PatchSubsetServerImplWithCodepointRemappingTest
    : public PatchSubsetServerImplTestBase {
 protected:
  PatchSubsetServerImplWithCodepointRemappingTest()
      : compressed_list_checksum_(new MockCompressedListChecksum()),
        server_(
            50, std::unique_ptr<FontProvider>(font_provider_),
            std::unique_ptr<Subsetter>(new FakeSubsetter()),
            std::unique_ptr<BinaryDiff>(binary_diff_),
            std::unique_ptr<Hasher>(hasher_),
            std::unique_ptr<CodepointMapper>(new SimpleCodepointMapper()),
            std::unique_ptr<CompressedListChecksum>(compressed_list_checksum_),
            std::unique_ptr<CodepointPredictor>(codepoint_predictor_)),
        set_abcd_encoded_(make_hb_set_from_ranges(1, 0, 3)),
        set_ab_encoded_(make_hb_set_from_ranges(1, 0, 1)) {}

  void ExpectCodepointMappingChecksum(std::vector<int> mapping_deltas,
                                      uint64_t checksum) {
    CompressedListProto compressed_list;
    for (int delta : mapping_deltas) {
      compressed_list.add_deltas(delta);
    }

    EXPECT_CALL(*compressed_list_checksum_,
                Checksum(EqualsProto(compressed_list)))
        .WillRepeatedly(Return(checksum));
  }

  MockCompressedListChecksum* compressed_list_checksum_;
  PatchSubsetServerImpl server_;

  hb_set_unique_ptr set_abcd_encoded_;
  hb_set_unique_ptr set_ab_encoded_;
};

// TODO(garretrieger): subsetter failure test.

TEST_F(PatchSubsetServerImplTest, NewRequest) {
  ExpectRoboto();
  ExpectDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_TRUE(response.patch().empty());
  EXPECT_EQ(response.replacement(), "Roboto-Regular.ttf:abcd");
  EXPECT_EQ(response.patched_checksum(), 43);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  EXPECT_FALSE(response.has_codepoint_ordering());
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest,
       NewRequestWithCodepointRemapping) {
  ExpectRoboto();
  ExpectDiff();

  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 43);

  ExpectCodepointMappingChecksum({97, 1, 1, 1, 1, 1}, 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  // Check that a codepoint mapping response has been included.
  EXPECT_EQ(response.ordering_checksum(), 44);
  EXPECT_EQ(response.codepoint_ordering().deltas_size(), 6);

  EXPECT_EQ(response.codepoint_ordering().deltas(0), 97);
  for (int i = 1; i < 6; i++) {
    EXPECT_EQ(response.codepoint_ordering().deltas(i), 1);
  }
}

TEST_F(PatchSubsetServerImplTest, PatchRequest) {
  ExpectRoboto();
  ExpectDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());
  request.set_original_font_checksum(42);
  request.set_base_checksum(43);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_EQ(response.patch(),
            "Roboto-Regular.ttf:abcd - Roboto-Regular.ttf:ab");
  EXPECT_EQ(response.patched_checksum(), 44);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  EXPECT_FALSE(response.has_codepoint_ordering());
}

TEST_F(PatchSubsetServerImplTest, PatchRequestWithCodepointPrediction) {
  ExpectRoboto();
  ExpectDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcde", 44);

  hb_set_unique_ptr font_codepoints = make_hb_set_from_ranges(1, 0x61, 0x66);
  hb_set_unique_ptr have_codepoints = make_hb_set(2, 0x61, 0x62);
  hb_set_unique_ptr requested_codepoints = make_hb_set(2, 0x63, 0x64);
  hb_set_unique_ptr codepoints_to_add = make_hb_set(1, 'e');
  AddPredictedCodepoints(font_codepoints.get(), have_codepoints.get(),
                         requested_codepoints.get(), codepoints_to_add.get());

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());
  request.set_original_font_checksum(42);
  request.set_base_checksum(43);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_EQ(response.patch(),
            "Roboto-Regular.ttf:abcde - Roboto-Regular.ttf:ab");
  EXPECT_EQ(response.patched_checksum(), 44);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  EXPECT_FALSE(response.has_codepoint_ordering());
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest,
       PatchRequestWithCodepointRemapping) {
  ExpectRoboto();
  ExpectDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 44);
  ExpectCodepointMappingChecksum({97, 1, 1, 1, 1, 1}, 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_encoded_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_encoded_,
                        request.mutable_codepoints_needed());
  request.set_original_font_checksum(42);
  request.set_base_checksum(43);
  request.set_ordering_checksum(44);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_EQ(response.patch(),
            "Roboto-Regular.ttf:abcd - Roboto-Regular.ttf:ab");
  EXPECT_EQ(response.patched_checksum(), 44);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);

  // Patch request should not send back a codepoint remapping.
  EXPECT_FALSE(response.has_codepoint_ordering());
}

TEST_F(PatchSubsetServerImplWithCodepointRemappingTest, BadIndexChecksum) {
  ExpectRoboto();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectCodepointMappingChecksum({97, 1, 1, 1, 1, 1}, 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());
  request.set_original_font_checksum(42);
  request.set_base_checksum(43);
  request.set_ordering_checksum(123);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);

  // Re-index should have no patch, but contain a codepoint mapping.
  EXPECT_TRUE(response.patch().empty());
  EXPECT_TRUE(response.replacement().empty());
  EXPECT_EQ(response.ordering_checksum(), 44);

  EXPECT_EQ(response.codepoint_ordering().deltas_size(), 6);
  EXPECT_EQ(response.codepoint_ordering().deltas(0), 97);
  for (int i = 1; i < 6; i++) {
    EXPECT_EQ(response.codepoint_ordering().deltas(i), 1);
  }
}

TEST_F(PatchSubsetServerImplTest, BadOriginalFontChecksum) {
  ExpectRoboto();
  ExpectDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());
  request.set_original_font_checksum(100);
  request.set_base_checksum(43);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_EQ(response.replacement(), "Roboto-Regular.ttf:abcd");
  EXPECT_EQ(response.patched_checksum(), 44);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchSubsetServerImplTest, BadBaseChecksum) {
  ExpectRoboto();
  ExpectDiff();
  ExpectChecksum("Roboto-Regular.ttf", 42);
  ExpectChecksum("Roboto-Regular.ttf:ab", 43);
  ExpectChecksum("Roboto-Regular.ttf:abcd", 44);

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_ab_, request.mutable_codepoints_have());
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());
  request.set_original_font_checksum(42);
  request.set_base_checksum(100);

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kOk);
  EXPECT_EQ(response.original_font_checksum(), 42);
  EXPECT_EQ(response.replacement(), "Roboto-Regular.ttf:abcd");
  EXPECT_EQ(response.patched_checksum(), 44);
  EXPECT_EQ(response.format(), PatchFormat::BROTLI_SHARED_DICT);
}

TEST_F(PatchSubsetServerImplTest, NotFound) {
  EXPECT_CALL(*font_provider_, GetFont("Roboto-Regular.ttf", _))
      .Times(1)
      .WillRepeatedly(Return(StatusCode::kNotFound));

  PatchRequestProto request;
  PatchResponseProto response;
  CompressedSet::Encode(*set_abcd_, request.mutable_codepoints_needed());

  EXPECT_EQ(server_.Handle("Roboto-Regular.ttf", request, &response),
            StatusCode::kNotFound);
}

}  // namespace patch_subset
