#include <iterator>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "common/axis_range.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/ift_client.h"
#include "ift/proto/patch_map.h"

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StrCat;
using common::AxisRange;
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
using ift::proto::PatchMap;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsSupersetOf;
using ::testing::Not;

namespace ift {

constexpr hb_tag_t kWdth = HB_TAG('w', 'd', 't', 'h');
constexpr hb_tag_t kWght = HB_TAG('w', 'g', 'h', 't');

class IntegrationTest : public ::testing::Test {
 protected:
  IntegrationTest() {
    // Noto Sans JP
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

    // Noto Sans JP VF
    blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP[wght].subset.ttf"));
    face = make_hb_face(hb_face_create(blob.get(), 0));
    noto_sans_vf_.set(face.get());

    vf_iftb_patches_.resize(5);
    for (int i = 1; i <= 4; i++) {
      std::string name = StrCat(
          "ift/testdata/NotoSansJP[wght].subset_iftb/outline-chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      vf_iftb_patches_[i].set(blob.get());
    }

    // Feature Test
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

    blob = make_hb_blob(hb_blob_create_from_file(
        "patch_subset/testdata/Roboto[wdth,wght].ttf"));
    face = make_hb_face(hb_face_create(blob.get(), 0));
    roboto_vf_.set(face.get());
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
    encoder.SetUrlTemplate("0x$2$1");
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

  Status InitEncoderForVfIftb(Encoder& encoder) {
    encoder.SetUrlTemplate("0x$2$1");
    {
      hb_face_t* face = noto_sans_vf_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }
    auto sc = encoder.SetId({0x479bb4b0, 0x20226239, 0xa7799c0f, 0x24275be0});
    if (!sc.ok()) {
      return sc;
    }

    for (uint i = 1; i < vf_iftb_patches_.size(); i++) {
      auto sc = encoder.AddExistingIftbPatch(i, vf_iftb_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  Status InitEncoderForIftbFeatureTest(Encoder& encoder) {
    encoder.SetUrlTemplate("0x$2$1");
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
    encoder.SetUrlTemplate("0x$2$1");
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

  Status InitEncoderForVf(Encoder& encoder) {
    encoder.SetUrlTemplate("0x$2$1");
    {
      auto face = roboto_vf_.face();
      encoder.SetFace(face.get());
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
      uint32_t id_value = std::stoul(id, nullptr, 16);
      FontData patch_data;
      if (id_value < iftb_patches.size()) {
        patch_data.shallow_copy(iftb_patches[id_value]);
      } else {
        auto it = encoder.Patches().find(id_value);
        if (it == encoder.Patches().end()) {
          return absl::InternalError(
              StrCat("Patch ", id_value, " was not found."));
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
      uint32_t id_value = std::stoul(id, nullptr, 16);
      FontData patch_data;
      auto it = encoder.Patches().find(id_value);
      if (it == encoder.Patches().end()) {
        return absl::InternalError(
            StrCat("Patch ", id_value, " was not found."));
      }
      patch_data.shallow_copy(it->second);
      client.AddPatch(id, patch_data);
    }

    return absl::OkStatus();
  }

  bool GvarHasLongOffsets(const FontData& font) {
    auto face = font.face();
    auto gvar_data =
        FontHelper::TableData(face.get(), HB_TAG('g', 'v', 'a', 'r'));
    if (gvar_data.size() < 16) {
      return false;
    }
    uint8_t flags_1 = gvar_data.str().at(15);
    return flags_1 == 0x01;
  }

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;

  FontData noto_sans_vf_;
  std::vector<FontData> vf_iftb_patches_;

  FontData feature_test_;
  std::vector<FontData> feature_test_patches_;

  FontData roboto_vf_;

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
// TODO(garretrieger): extension specific url template.

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

  client->AddDesiredCodepoints({0x49});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
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

  client->AddDesiredCodepoints({0x49, 0x4F});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // Phase 1
  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
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
  encoder.AddExtensionSubset({0x4E, 0x4F});
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

  client->AddDesiredCodepoints({0x49});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  flat_hash_set<std::string> patches_expected = {"0x01"};
  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches, patches_expected);

  client->AddDesiredCodepoints({0x4E, 0x4F});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  patches_expected = {"0x01"};
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches, patches_expected);

  // Patch resolution
  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
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

TEST_F(IntegrationTest,
       SharedBrotli_DesignSpaceAugmentation_IgnoresDesignSpace) {
  Encoder encoder;
  auto sc = InitEncoderForVf(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  Encoder::SubsetDefinition def{'a', 'b', 'c'};
  def.design_space[kWdth] = AxisRange::Point(100.0f);
  sc = encoder.SetBaseSubsetFromDef(def);

  encoder.AddExtensionSubset({'d', 'e', 'f'});
  encoder.AddExtensionSubset({'h', 'i', 'j'});
  encoder.AddOptionalDesignSpace({{kWdth, *AxisRange::Range(75.0f, 100.0f)}});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  auto face = encoded->face();
  auto ds = FontHelper::GetDesignSpace(face.get());
  flat_hash_map<hb_tag_t, AxisRange> expected_ds{
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  client->AddDesiredCodepoints({'e'});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);

  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  ds = FontHelper::GetDesignSpace(face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c', 'd', 'e', 'f'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('h')), Not(Contains('i')),
                                Not(Contains('j'))));
}

TEST_F(IntegrationTest, SharedBrotli_DesignSpaceAugmentation) {
  Encoder encoder;
  auto sc = InitEncoderForVf(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  Encoder::SubsetDefinition def{'a', 'b', 'c'};
  def.design_space[kWdth] = AxisRange::Point(100.0f);
  sc = encoder.SetBaseSubsetFromDef(def);

  encoder.AddExtensionSubset({'d', 'e', 'f'});
  encoder.AddExtensionSubset({'h', 'i', 'j'});
  encoder.AddOptionalDesignSpace({{kWdth, *AxisRange::Range(75.0f, 100.0f)}});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto codepoints = ToCodepointsSet(*encoded);
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  auto face = encoded->face();
  auto ds = FontHelper::GetDesignSpace(face.get());
  flat_hash_map<hb_tag_t, AxisRange> expected_ds{
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  // Phase 1
  client->AddDesiredCodepoints({'b'});
  sc.Update(client->AddDesiredDesignSpace(kWdth, 80.0f, 80.0f));
  ASSERT_TRUE(sc.ok()) << sc;
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);
  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  ds = FontHelper::GetDesignSpace(face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
      {kWdth, *AxisRange::Range(75, 100)},
  };
  ASSERT_EQ(*ds, expected_ds);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  // Phase 2
  client->AddDesiredCodepoints({'e'});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);
  sc = AddPatchesSbr(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c', 'd', 'e', 'f'}));

  face = client->GetFontData().face();
  ds = FontHelper::GetDesignSpace(face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
      {kWdth, *AxisRange::Range(75, 100)},
  };
  ASSERT_EQ(*ds, expected_ds);
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

  client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 3);  // 1 shared brotli and 2 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
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

  client->AddDesiredCodepoints({chunk2_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, feature_test_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
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

  client->AddDesiredFeatures({kVrt3});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);
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

  client->AddDesiredCodepoints({chunk4_cp});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

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

  client->AddDesiredCodepoints({chunk3_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  uint32_t gid_count_2 = hb_face_get_glyph_count(face.get());

  // ### Phase 2 ###
  client->AddDesiredCodepoints({chunk2_cp});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

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
  client->AddDesiredCodepoints({chunk1_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Phase 2
  client->AddDesiredCodepoints({chunk3_cp});
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

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

  client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 3);  // 1 shared brotli and 2 iftb.

  sc = AddPatchesIftb(*client, encoder, iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
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

TEST_F(IntegrationTest, MixedMode_DesignSpaceAugmentation) {
  Encoder encoder;
  auto sc = InitEncoderForVfIftb(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3, 4}} + add wght axis
  sc = encoder.SetBaseSubsetFromIftbPatches({1},
                                            {{kWght, AxisRange::Point(100)}});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  encoder.AddOptionalDesignSpace({{kWght, *AxisRange::Range(100, 900)}});
  encoder.AddIftbUrlTemplateOverride({{kWght, *AxisRange::Range(100, 900)}},
                                     "vf-0x$2$1");

  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  // Phase 1: non VF augmentation.
  client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  flat_hash_set<std::string> expected_patches = {"0x03", "0x04", "0x06"};
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatchesIftb(*client, encoder, vf_iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Phase 2: VF augmentation.
  sc = client->AddDesiredDesignSpace(kWght, 100, 900);
  ASSERT_TRUE(sc.ok()) << sc;
  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  patches = client->PatchesNeeded();
  expected_patches = {
      "0x0d",
  };
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatchesIftb(*client, encoder, vf_iftb_patches_);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  ASSERT_TRUE(GvarHasLongOffsets(client->GetFontData()));
  // TODO(garretrieger): check gvar only has coverage of base suset glyphs

  patches = client->PatchesNeeded();
  expected_patches = {"vf-0x03", "vf-0x04"};
  ASSERT_EQ(patches, expected_patches);

  // TODO(garretrieger): apply the vf patches and test
  //  gvar should now have data for all augmented glyphs.
}

}  // namespace ift
