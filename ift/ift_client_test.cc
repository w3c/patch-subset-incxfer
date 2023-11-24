#include "ift/ift_client.h"

#include "absl/container/btree_set.h"
#include "common/brotli_binary_diff.h"
#include "common/font_data.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "gtest/gtest.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::IsInvalidArgument;
using common::BrotliBinaryDiff;
using common::FontData;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;
using ift::proto::IFT;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::SHARED_BROTLI_ENCODING;

namespace ift {

static constexpr uint32_t kSampleFont = 0;
static constexpr uint32_t kComplexFont = 1;
static constexpr uint32_t kInvalidFont = 2;
static constexpr uint32_t kLeaf = 3;
static constexpr uint32_t kFontWithFeatures = 4;

static constexpr uint32_t kLiga = HB_TAG('l', 'i', 'g', 'a');
static constexpr uint32_t kLigaNo = 30;

class IFTClientTest : public ::testing::Test {
 protected:
  IFTClientTest() {
    // Simple Test Font
    IFT sample;
    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(8);
    m->set_patch_encoding(IFTB_ENCODING);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(32);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    sample.set_url_template("https://localhost/patches/$2$1.patch");

    // Complex Test Font
    IFT complex;
    complex.set_default_patch_encoding(IFTB_ENCODING);
    complex.set_url_template("$1.patch");

    m = complex.add_subset_mapping();
    set = make_hb_set(3, 4, 5, 6);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));

    m = complex.add_subset_mapping();
    set = make_hb_set(3, 6, 7, 8);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));

    m = complex.add_subset_mapping();
    set = make_hb_set(3, 9, 10, 11);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));

    m = complex.add_subset_mapping();
    set = make_hb_set(5, 11, 20, 21, 22, 23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    m = complex.add_subset_mapping();
    set = make_hb_set(3, 11, 12, 20);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    m = complex.add_subset_mapping();
    set = make_hb_set(3, 11, 12, 25);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    // Invalid Test Font
    IFT invalid;
    invalid.set_default_patch_encoding(IFTB_ENCODING);
    invalid.set_url_template("$1.patch");

    m = invalid.add_subset_mapping();
    set = make_hb_set(3, 4, 5, 6);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));

    m = invalid.add_subset_mapping();
    set = make_hb_set(3, 7, 8, 9);
    m->set_id_delta(-1);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));

    m = invalid.add_subset_mapping();
    set = make_hb_set(3, 4, 5);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(-1);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    // With Features
    IFT sample_with_features = sample;
    m = sample_with_features.add_subset_mapping();
    set = make_hb_set(3, 0, 1, 2);
    m->set_bias(20);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(10);  // 0x35 (53)
    m->add_feature_index(kLigaNo);
    m->set_patch_encoding(IFTB_ENCODING);

    m = sample_with_features.add_subset_mapping();
    set = make_hb_set(2, 0, 1);
    m->set_bias(60);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);  // 0x36 (54)
    m->add_feature_index(kLigaNo);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    m = sample_with_features.add_subset_mapping();
    set = make_hb_set(4, 0, 1, 2, 3);
    m->set_bias(70);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);  // 0x37 (55)
    m->add_feature_index(kLigaNo);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    m = sample_with_features.add_subset_mapping();
    set = make_hb_set(5, 0, 1, 2, 3, 4);
    m->set_bias(80);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);  // 0x38 (56)
    m->add_feature_index(kLigaNo);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    m = sample_with_features.add_subset_mapping();
    m->set_id_delta(7);  // 0x40 (64)
    m->add_feature_index(kLigaNo);
    m->set_patch_encoding(IFTB_ENCODING);

    hb_blob_t* blob =
        hb_blob_create_from_file("patch_subset/testdata/Roboto-Regular.ab.ttf");
    hb_face_t* face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
    fonts[kLeaf].set(face);

    auto font = IFTTable::AddToFont(face, sample);
    auto new_face = font->face();
    fonts[kSampleFont].set(new_face.get());

    font = IFTTable::AddToFont(face, complex);
    new_face = font->face();
    fonts[kComplexFont].set(new_face.get());

    font = IFTTable::AddToFont(face, invalid);
    new_face = font->face();
    fonts[kInvalidFont].set(new_face.get());

    font = IFTTable::AddToFont(face, sample_with_features);
    new_face = font->face();
    fonts[kFontWithFeatures].set(new_face.get());

    iftb_font = from_file("ift/testdata/NotoSansJP-Regular.ift.ttf");
    chunk1 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk1.br");

    hb_face_destroy(face);
  }

  FontData from_file(const char* filename) {
    hb_blob_t* blob = hb_blob_create_from_file(filename);
    FontData result(blob);
    hb_blob_destroy(blob);
    return result;
  }

  FontData fonts[5];

  FontData iftb_font;
  FontData chunk1;
};

typedef flat_hash_set<uint32_t> uint_set;
struct PatchesNeededTestCase {
  PatchesNeededTestCase(uint32_t font_id_, uint_set codepoints_,
                        uint_set features_, uint_set expected_)
      : font_id(font_id_),
        codepoints(codepoints_),
        features(features_),
        expected(expected_) {}

  uint32_t font_id;
  uint_set codepoints;
  uint_set features;
  uint_set expected;
};

class IFTClientParameterizedTest
    : public IFTClientTest,
      public testing::WithParamInterface<PatchesNeededTestCase> {};

TEST_P(IFTClientParameterizedTest, PatchesNeeded) {
  auto p = GetParam();

  FontData font;
  font.copy(fonts[p.font_id].str());

  auto client = IFTClient::NewClient(std::move(font));
  ASSERT_TRUE(client.ok()) << client.status();

  if (!p.codepoints.empty()) {
    auto s = client->AddDesiredCodepoints(p.codepoints);
    ASSERT_TRUE(s.ok()) << s;
  }

  if (!p.features.empty()) {
    auto s = client->AddDesiredFeatures(p.features);
    ASSERT_TRUE(s.ok()) << s;
  }

  ASSERT_EQ(client->PatchesNeeded(), p.expected);
}

INSTANTIATE_TEST_SUITE_P(
    TargetCodepoints, IFTClientParameterizedTest,
    testing::Values(
        PatchesNeededTestCase(kSampleFont, {30}, {}, {0x09}),
        PatchesNeededTestCase(kSampleFont, {55, 57}, {}, {0x2a}),
        PatchesNeededTestCase(kSampleFont, {32, 56}, {}, {0x09, 0x2a}),
        PatchesNeededTestCase(kSampleFont, {32, 56}, {kLiga}, {0x09, 0x2a}),
        PatchesNeededTestCase(kSampleFont, {}, {}, {}),
        PatchesNeededTestCase(kSampleFont, {112}, {}, {}),
        PatchesNeededTestCase(kSampleFont, {30, 112}, {}, {0x09}),

        PatchesNeededTestCase(kFontWithFeatures, {21}, {kLiga}, {0x35, 0x40}),
        PatchesNeededTestCase(kFontWithFeatures, {32, 56, 21}, {kLiga},
                              {0x09, 0x2a, 0x35, 0x40}),
        PatchesNeededTestCase(kFontWithFeatures, {32, 56}, {kLiga},
                              {0x09, 0x2a, 0x40}),
        PatchesNeededTestCase(kFontWithFeatures, {100}, {kLiga}, {0x40}),

        // dependent entry prioritization:
        // - goes the the largest intersection with ties broken by smaller entry
        // size
        // - Input has the following entries
        //   IFTB {30, 32},            0x09
        //   SBR  {55, 56, 57},        0x2a
        //   IFTB {20, 21, 22},        0x35
        //   SBR  {60, 61},            0x36
        //   SBR  {70, 71, 72, 73}     0x37
        //   SBR  {80, 81, 82, 83, 84} 0x38
        PatchesNeededTestCase(kFontWithFeatures, {60, 70, 71}, {kLiga},
                              {0x37, 0x40}),
        PatchesNeededTestCase(kFontWithFeatures, {60, 61, 70}, {kLiga},
                              {0x36, 0x40}),
        PatchesNeededTestCase(kFontWithFeatures, {60, 61, 70, 71}, {kLiga},
                              {0x36, 0x40}),

        PatchesNeededTestCase(kComplexFont, {4, 6}, {}, {1, 2}),
        PatchesNeededTestCase(kComplexFont, {4, 11, 12}, {}, {1, 3, 5}),
        PatchesNeededTestCase(kComplexFont, {12}, {}, {5}),

        PatchesNeededTestCase(kComplexFont, {5, 100}, {}, {1}),
        PatchesNeededTestCase(kComplexFont, {100}, {}, {}),

        PatchesNeededTestCase(kInvalidFont, {6}, {}, {1}),
        PatchesNeededTestCase(kInvalidFont, {6, 8}, {}, {1}),

        PatchesNeededTestCase(kLeaf, {30}, {}, {})));

TEST_F(IFTClientTest, PatchUrls_InvalidRepeatedPatchIndices) {
  auto client = IFTClient::NewClient(std::move(fonts[kInvalidFont]));
  ASSERT_TRUE(client.ok()) << client.status();

  hb_set_unique_ptr codepoints = make_hb_set(1, 4);
  auto s = client->AddDesiredCodepoints({4});
  ASSERT_TRUE(absl::IsInternal(s)) << s;
}

TEST_F(IFTClientTest, ApplyPatches_IFTB) {
  std::vector<FontData> patches;
  patches.emplace_back().shallow_copy(chunk1);

  auto client = IFTClient::NewClient(std::move(iftb_font));
  ASSERT_TRUE(client.ok()) << client.status();

  auto s = client->AddDesiredCodepoints({0xb7});
  ASSERT_TRUE(s.ok()) << s;

  uint_set expected = {1};
  ASSERT_EQ(client->PatchesNeeded(), expected);
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  client->AddPatch(1, patches[0]);

  ASSERT_TRUE(client->PatchesNeeded().empty());

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);
}

TEST_F(IFTClientTest, PatchToUrl_NoFormatters) {
  std::string url("https://localhost/abc.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/abc.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/abc.patch");
}

TEST_F(IFTClientTest, PatchToUrl_InvalidFormatter) {
  std::string url("https://localhost/$1.$patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/0.$patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/5.$patch");

  url = "https://localhost/$1.patch$";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/0.patch$");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/5.patch$");

  url = "https://localhost/$1.pa$$2tch";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/0.pa$0tch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/5.pa$0tch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 18), "https://localhost/2.pa$1tch");

  url = "https://localhost/$6.patch";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/$6.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/$6.patch");

  url = "https://localhost/$12.patch";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/02.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/52.patch");
}

TEST_F(IFTClientTest, PatchToUrl_Basic) {
  std::string url = "https://localhost/$2$1.patch";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/00.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/05.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 12), "https://localhost/0c.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 18), "https://localhost/12.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 212), "https://localhost/d4.patch");

  url = "https://localhost/$2$1";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/00");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/05");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 12), "https://localhost/0c");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 18), "https://localhost/12");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 212), "https://localhost/d4");

  url = "$2$1.patch";
  ;
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "00.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "05.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 12), "0c.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 18), "12.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 212), "d4.patch");

  url = "$1";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "0");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "5");
}

TEST_F(IFTClientTest, PatchToUrl_Complex) {
  std::string url = "https://localhost/$5/$3/$3$2$1.patch";
  EXPECT_EQ(IFTClient::PatchToUrl(url, 0), "https://localhost/0/0/000.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 5), "https://localhost/0/0/005.patch");
  EXPECT_EQ(IFTClient::PatchToUrl(url, 200000),
            "https://localhost/3/d/d40.patch");
}

}  // namespace ift
