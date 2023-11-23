#include <iterator>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/ift_client.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::Status;
using absl::StrCat;
using common::FontData;
using common::FontHelper;
using common::hb_blob_unique_ptr;
using common::hb_face_unique_ptr;
using common::hb_set_unique_ptr;
using common::make_hb_blob;
using common::make_hb_face;
using common::make_hb_set;
using ift::IFTClient;
using ift::encoder::Encoder;
using ift::proto::IFTB_ENCODING;
using ift::proto::PatchEncoding;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;

namespace ift {

class IntegrationTest : public ::testing::Test {
 protected:
  IntegrationTest() {
    auto blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP-Regular.subset.ttf"));
    auto face = make_hb_face(hb_face_create(blob.get(), 0));
    noto_sans_jp_.set(face.get());

    iftb_patches_.resize(5);
    for (int i = 1; i <= 4; i++) {
      std::string name =
          StrCat("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      iftb_patches_[i].set(blob.get());
    }

    blob = make_hb_blob(hb_blob_create_from_file(
        "ift/testdata/NotoSansJP-Regular.feature-test.ttf"));
    face = make_hb_face(hb_face_create(blob.get(), 0));
    feature_test_.set(face.get());

    feature_test_patches_.resize(7);
    for (int i = 1; i <= 6; i++) {
      std::string name = StrCat(
          "ift/testdata/NotoSansJP-Regular.feature-test_iftb/chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      feature_test_patches_[i].set(blob.get());
    }
  }

  btree_set<uint32_t> ToCodepointsSet(const FontData& font_data) {
    hb_face_t* face = font_data.reference_face();

    hb_set_unique_ptr codepoints = make_hb_set();
    hb_face_collect_unicodes(face, codepoints.get());
    hb_face_destroy(face);

    btree_set<uint32_t> result;
    hb_codepoint_t cp = HB_SET_VALUE_INVALID;
    while (hb_set_next(codepoints.get(), &cp)) {
      result.insert(cp);
    }

    return result;
  }

  Status InitEncoderForIftb(Encoder& encoder) {
    encoder.SetUrlTemplate("$2$1");
    {
      hb_face_t* face = noto_sans_jp_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }
    auto sc = encoder.SetId({0x3c2bfda0, 0x890625c9, 0x40c644de, 0xb1195627});
    if (!sc.ok()) {
      return sc;
    }

    for (uint i = 1; i < iftb_patches_.size(); i++) {
      auto sc = encoder.AddExistingIftbPatch(i, iftb_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  Status InitEncoderForIftbFeatureTest(Encoder& encoder) {
    encoder.SetUrlTemplate("$2$1");
    {
      hb_face_t* face = feature_test_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }
    auto sc = encoder.SetId({0xd673ad42, 0x775df247, 0xabdacfb5, 0x3e1543eb});
    if (!sc.ok()) {
      return sc;
    }

    for (uint i = 1; i < feature_test_patches_.size(); i++) {
      auto sc = encoder.AddExistingIftbPatch(i, feature_test_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  Status InitEncoderForSharedBrotli(Encoder& encoder) {
    encoder.SetUrlTemplate("$2$1");
    {
      hb_face_t* face = noto_sans_jp_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }
    auto sc = encoder.SetId({0x01, 0x02, 0x03, 0x04});
    if (!sc.ok()) {
      return sc;
    }

    return absl::OkStatus();
  }

  Status AddPatchesIftb(IFTClient& client, Encoder& encoder,
                        const std::vector<FontData>& iftb_patches) {
    auto patches = client.PatchesNeeded();
    for (const auto& id : patches) {
      FontData patch_data;
      if (id < iftb_patches.size()) {
        patch_data.shallow_copy(iftb_patches[id]);
      } else {
        auto it = encoder.Patches().find(id);
        if (it == encoder.Patches().end()) {
          return absl::InternalError(StrCat("Patch ", id, " was not found."));
        }
        patch_data.shallow_copy(it->second);
      }

      client.AddPatch(id, patch_data);
    }

    return absl::OkStatus();
  }

  Status AddPatchesSbr(IFTClient& client, Encoder& encoder) {
    auto patches = client.PatchesNeeded();
    for (const auto& id : patches) {
      FontData patch_data;
      auto it = encoder.Patches().find(id);
      if (it == encoder.Patches().end()) {
        return absl::InternalError(StrCat("Patch ", id, " was not found."));
      }
      patch_data.shallow_copy(it->second);
      client.AddPatch(id, patch_data);
    }

    return absl::OkStatus();
  }

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;

  FontData feature_test_;
  std::vector<FontData> feature_test_patches_;

  uint32_t chunk0_cp = 0x47;
  uint32_t chunk1_cp = 0xb7;
  uint32_t chunk2_cp = 0xb2;
  uint32_t chunk3_cp = 0xeb;
  uint32_t chunk4_cp = 0xa8;

  uint32_t chunk0_gid = 40;
  uint32_t chunk1_gid = 117;
  uint32_t chunk2_gid = 112;
  uint32_t chunk2_gid_non_cmapped = 900;
  uint32_t chunk3_gid = 169;
  uint32_t chunk4_gid = 103;

  static constexpr hb_tag_t kVrt3 = HB_TAG('v', 'r', 't', '3');
};

// TODO(garretrieger): add IFTB only test case.
// TODO(garretrieger): add test that uses feature tags.

TEST_F(IntegrationTest, SharedBrotliOnly) {
  Encoder encoder;
  auto sc = InitEncoderForSharedBrotli(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset({0x45, 0x46, 0x47});
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({0x49});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_TRUE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));
}

TEST_F(IntegrationTest, SharedBrotliMultiple) {
  Encoder encoder;
  auto sc = InitEncoderForSharedBrotli(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset({0x45, 0x46, 0x47});
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({0x49, 0x4F});
  ASSERT_TRUE(sc.ok()) << sc;

  // Phase 1
  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // Phase 2
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_TRUE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_TRUE(codepoints.contains(0x4E));
}

TEST_F(IntegrationTest, SharedBrotli_AddCodepointsWhileInProgress) {
  Encoder encoder;
  auto sc = InitEncoderForSharedBrotli(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset({0x45, 0x46, 0x47});
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50, 0x51});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({0x49});
  ASSERT_TRUE(sc.ok()) << sc;

  flat_hash_set<uint32_t> patches_expected = {1};
  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches, patches_expected);

  sc = client->AddDesiredCodepoints({0x4F});
  ASSERT_TRUE(sc.ok()) << sc;

  patches_expected = {3};
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches, patches_expected);

  // Patch resolution
  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_TRUE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_TRUE(codepoints.contains(0x4E));
}

TEST_F(IntegrationTest, MixedMode) {
  Encoder encoder;
  auto sc = InitEncoderForIftb(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3, 4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({1});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 3);  // 1 shared brotli and 2 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));

  auto face = client->GetFontData().face();
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk0_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk1_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_FALSE(
      !FontHelper::GlyfData(face.get(), chunk2_gid_non_cmapped)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk3_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk4_gid)->empty());
}

TEST_F(IntegrationTest, MixedMode_OptionalFeatureTags) {
  Encoder encoder;
  auto sc = InitEncoderForIftbFeatureTest(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0}, {1}, {2}, {3}, {4}}
  // With optional feature chunks for vrt3:
  //   1, 2 -> 5
  //   4    -> 6
  sc = encoder.SetBaseSubsetFromIftbPatches({});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({1}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({4}));
  sc.Update(encoder.AddIftbFeatureSpecificPatch(1, 5, kVrt3));
  sc.Update(encoder.AddIftbFeatureSpecificPatch(2, 5, kVrt3));
  sc.Update(encoder.AddIftbFeatureSpecificPatch(4, 6, kVrt3));
  encoder.AddOptionalFeatureGroup({kVrt3});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({chunk2_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, feature_test_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  auto face = client->GetFontData().face();
  auto feature_tags = FontHelper::GetFeatureTags(face.get());
  ASSERT_FALSE(feature_tags.contains(kVrt3));

  static constexpr uint32_t chunk2_gid = 816;
  static constexpr uint32_t chunk4_gid = 800;
  static constexpr uint32_t chunk5_gid = 989;
  static constexpr uint32_t chunk6_gid = 932;
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(face.get(), chunk5_gid)->empty());

  sc = client->AddDesiredFeatures({kVrt3});
  sc.Update(AddPatchesIftb(*client, encoder, feature_test_patches_));
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  feature_tags = FontHelper::GetFeatureTags(face.get());
  ASSERT_TRUE(feature_tags.contains(kVrt3));
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(face.get(), chunk4_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk5_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(face.get(), chunk6_gid)->empty());

  sc = client->AddDesiredCodepoints({chunk4_cp});
  ASSERT_TRUE(sc.ok()) << sc;
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 3);  // 2 shared brotli and 1 iftb.

  sc.Update(AddPatchesIftb(*client, encoder, feature_test_patches_));
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk4_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk5_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(face.get(), chunk6_gid)->empty());
}

TEST_F(IntegrationTest, MixedMode_LocaLenChange) {
  Encoder encoder;
  auto sc = InitEncoderForIftb(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0}, {1}, {2}, {3}, {4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({1}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  // ### Phase 1 ###
  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();
  auto face = client->GetFontData().face();
  uint32_t gid_count_1 = hb_face_get_glyph_count(face.get());

  sc = client->AddDesiredCodepoints({chunk3_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  uint32_t gid_count_2 = hb_face_get_glyph_count(face.get());

  // ### Phase 2 ###
  sc = client->AddDesiredCodepoints({chunk2_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  uint32_t gid_count_3 = hb_face_get_glyph_count(face.get());

  // ### Checks ###

  // To avoid loca len change the encoder ensures that a full len
  // loca exists in the base font. So gid count should be consistent
  // at each point
  ASSERT_EQ(gid_count_1, gid_count_2);
  ASSERT_EQ(gid_count_2, gid_count_3);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_TRUE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk0_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(face.get(), chunk1_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk3_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(face.get(), chunk4_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), gid_count_3 - 1)->empty());
}

TEST_F(IntegrationTest, MixedMode_Complex) {
  Encoder encoder;
  auto sc = InitEncoderForIftb(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0}, {1, 2}, {3, 4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({1, 2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  // Phase 1
  sc = client->AddDesiredCodepoints({chunk1_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Phase 2
  sc = client->AddDesiredCodepoints({chunk3_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Check the results
  auto codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_TRUE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));

  auto face = client->GetFontData().face();
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk0_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk1_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(face.get(), chunk3_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(face.get(), chunk4_gid)->empty());
}

TEST_F(IntegrationTest, MixedMode_SequentialDependentPatches) {
  Encoder encoder;
  auto sc = InitEncoderForIftb(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3}, {4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({1});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  sc = client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 3);  // 1 shared brotli and 2 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // the first application round would have added one of {3}
  // and {4}. now that one is applied, the second is still needed.
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);  // 1 shared brotli

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  auto codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));
}

}  // namespace ift
