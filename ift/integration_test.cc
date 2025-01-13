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
#include "ift/client/fontations_client.h"
#include "ift/encoder/encoder.h"
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
using ift::client::Extend;
using ift::client::ExtendWithDesignSpace;
using ift::encoder::Encoder;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
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
    noto_sans_jp_.set(blob.get());

    iftb_patches_.resize(5);
    for (int i = 1; i <= 4; i++) {
      std::string name =
          StrCat("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      iftb_patches_[i].set(blob.get());
    }

    /*
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
    */

    // Feature Test
    blob = make_hb_blob(hb_blob_create_from_file(
        "ift/testdata/NotoSansJP-Regular.feature-test.ttf"));
    feature_test_.set(blob.get());

    feature_test_patches_.resize(7);
    for (int i = 1; i <= 6; i++) {
      std::string name = StrCat(
          "ift/testdata/NotoSansJP-Regular.feature-test_iftb/chunk", i, ".br");
      blob = make_hb_blob(hb_blob_create_from_file(name.c_str()));
      assert(hb_blob_get_length(blob.get()) > 0);
      feature_test_patches_[i].set(blob.get());
    }

    blob = make_hb_blob(
        hb_blob_create_from_file("common/testdata/Roboto[wdth,wght].ttf"));
    roboto_vf_.set(blob.get());
  }

  Status InitEncoderForMixedMode(Encoder& encoder) {
    encoder.SetUrlTemplate("{id}");
    {
      hb_face_t* face = noto_sans_jp_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }

    for (uint i = 1; i < iftb_patches_.size(); i++) {
      auto sc = encoder.AddExistingIftbPatch(i, iftb_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  /*
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
  */

  Status InitEncoderForMixedModeFeatureTest(Encoder& encoder) {
    encoder.SetUrlTemplate("{id}");
    {
      hb_face_t* face = feature_test_.reference_face();
      encoder.SetFace(face);
      hb_face_destroy(face);
    }

    for (uint i = 1; i < feature_test_patches_.size(); i++) {
      auto sc = encoder.AddExistingIftbPatch(i, feature_test_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  Status InitEncoderForTableKeyed(Encoder& encoder) {
    encoder.SetUrlTemplate("{id}");
    auto face = noto_sans_jp_.face();
    encoder.SetFace(face.get());
    return absl::OkStatus();
  }

  Status InitEncoderForVf(Encoder& encoder) {
    encoder.SetUrlTemplate("{id}");
    {
      auto face = roboto_vf_.face();
      encoder.SetFace(face.get());
    }

    return absl::OkStatus();
  }

  /*
  Status AddPatches(IFTClient& client, Encoder& encoder) {
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
  */

  /*
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
  */

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;

  // FontData noto_sans_vf_;
  // std::vector<FontData> vf_iftb_patches_;

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

bool GlyphDataMatches(hb_face_t* a, hb_face_t* b, uint32_t codepoint) {
  uint32_t gid_a, gid_b;

  hb_font_t* font_a = hb_font_create(a);
  hb_font_t* font_b = hb_font_create(b);
  bool a_present = hb_font_get_nominal_glyph(font_a, codepoint, &gid_a);
  bool b_present = hb_font_get_nominal_glyph(font_b, codepoint, &gid_b);
  hb_font_destroy(font_a);
  hb_font_destroy(font_b);

  if (!a_present && !b_present) {
    return true;
  }

  if (a_present != b_present) {
    return false;
  }

  auto a_data = FontHelper::GlyfData(a, gid_a);
  auto b_data = FontHelper::GlyfData(b, gid_b);
  return *a_data == *b_data;
}

// TODO(garretrieger): full expansion test.
// TODO(garretrieger): test of a woff2 encoded IFT font.
// TODO(garretrieger): add IFTB only test case.
// TODO(garretrieger): extension specific url template.

TEST_F(IntegrationTest, TableKeyedOnly) {
  Encoder encoder;
  auto sc = InitEncoderForTableKeyed(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset({0x45, 0x46, 0x47});
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto encoded_face = encoded->face();
  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto extended = Extend(encoder, *encoded, {0x49});
  ASSERT_TRUE(extended.ok()) << extended.status();

  auto extended_face = extended->face();
  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_TRUE(codepoints.contains(0x48));
  ASSERT_TRUE(codepoints.contains(0x49));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto original_face = noto_sans_jp_.face();
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x41);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x48);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x49);
}

TEST_F(IntegrationTest, TableKeyedMultiple) {
  Encoder encoder;
  auto sc = InitEncoderForTableKeyed(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset({0x45, 0x46, 0x47});
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto encoded_face = encoded->face();
  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto extended = Extend(encoder, *encoded, {0x49, 0x4F});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_TRUE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_TRUE(codepoints.contains(0x4E));

  auto original_face = noto_sans_jp_.face();
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x41);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x45);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x48);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x4E);
}

TEST_F(IntegrationTest, TableKeyedWithOverlaps) {
  Encoder encoder;
  auto sc = InitEncoderForTableKeyed(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddExtensionSubset(
      {0x45, 0x46, 0x47, 0x48});  // 0x48 is in two subsets
  encoder.AddExtensionSubset({0x48, 0x49, 0x4A});
  encoder.AddExtensionSubset({0x4B, 0x4C, 0x4D});
  encoder.AddExtensionSubset({0x4E, 0x4F, 0x50});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  auto encoded_face = encoded->face();
  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_FALSE(codepoints.contains(0x45));
  ASSERT_FALSE(codepoints.contains(0x48));
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  auto extended = Extend(encoder, *encoded, {0x48});
  ASSERT_TRUE(extended.ok()) << extended.status();

  auto extended_face = extended->face();
  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(0x41));
  ASSERT_TRUE(codepoints.contains(0x48));
  auto original_face = noto_sans_jp_.face();

  // Extending for 0x48 should grab one and only one of the two possible
  // subsets, which specific one is client specific we just care that only one
  // was applied.
  if (codepoints.contains(0x45)) {
    GlyphDataMatches(original_face.get(), extended_face.get(), 0x45);
    ASSERT_FALSE(codepoints.contains(0x49));
  } else {
    ASSERT_TRUE(codepoints.contains(0x49));
    GlyphDataMatches(original_face.get(), extended_face.get(), 0x49);
  }
  ASSERT_FALSE(codepoints.contains(0x4B));
  ASSERT_FALSE(codepoints.contains(0x4E));

  GlyphDataMatches(original_face.get(), extended_face.get(), 0x41);
  GlyphDataMatches(original_face.get(), extended_face.get(), 0x48);
}

TEST_F(IntegrationTest, TableKeyed_DesignSpaceAugmentation_IgnoresDesignSpace) {
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
  auto encoded_face = encoded->face();

  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  auto ds = FontHelper::GetDesignSpace(encoded_face.get());
  flat_hash_map<hb_tag_t, AxisRange> expected_ds{
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  auto extended = Extend(encoder, *encoded, {'e'});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  ds = FontHelper::GetDesignSpace(extended_face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
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
  auto encoded_face = encoded->face();

  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  auto ds = FontHelper::GetDesignSpace(encoded_face.get());
  flat_hash_map<hb_tag_t, AxisRange> expected_ds{
      {kWght, *AxisRange::Range(100, 900)},
  };
  ASSERT_EQ(*ds, expected_ds);

  auto extended = ExtendWithDesignSpace(
      encoder, *encoded, {'b'}, {},
      {{HB_TAG('w', 'd', 't', 'h'), AxisRange::Point(80)}});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  ds = FontHelper::GetDesignSpace(extended_face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
      {kWdth, *AxisRange::Range(75, 100)},
  };
  ASSERT_EQ(*ds, expected_ds);

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c'}));
  ASSERT_THAT(codepoints, AllOf(Not(Contains('d')), Not(Contains('e')),
                                Not(Contains('f')), Not(Contains('h')),
                                Not(Contains('i')), Not(Contains('j'))));

  // Try extending the updated font again.
  extended = Extend(encoder, *extended, {'e'});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_THAT(codepoints, IsSupersetOf({'a', 'b', 'c', 'd', 'e', 'f'}));

  ds = FontHelper::GetDesignSpace(extended_face.get());
  expected_ds = {
      {kWght, *AxisRange::Range(100, 900)},
      {kWdth, *AxisRange::Range(75, 100)},
  };
  ASSERT_EQ(*ds, expected_ds);
}

TEST_F(IntegrationTest, MixedMode) {
  Encoder encoder;
  auto sc = InitEncoderForMixedMode(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3, 4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({1});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto encoded_face = encoded->face();

  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  auto extended = Extend(encoder, *encoded, {chunk3_cp, chunk4_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));

  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk0_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk1_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_FALSE(
      !FontHelper::GlyfData(extended_face.get(), chunk2_gid_non_cmapped)
           ->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk3_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk4_gid)->empty());

  auto original_face = noto_sans_jp_.face();
  GlyphDataMatches(original_face.get(), extended_face.get(), chunk0_gid);
  GlyphDataMatches(original_face.get(), extended_face.get(), chunk1_gid);
  GlyphDataMatches(original_face.get(), extended_face.get(), chunk3_gid);
  GlyphDataMatches(original_face.get(), extended_face.get(), chunk4_gid);
}

TEST_F(IntegrationTest, MixedMode_OptionalFeatureTags) {
  Encoder encoder;
  auto sc = InitEncoderForMixedModeFeatureTest(encoder);
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
  auto encoded_face = encoded->face();

  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  // Ext 1 - extend to {chunk2_cp}
  auto extended = Extend(encoder, *encoded, {chunk2_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  auto feature_tags = FontHelper::GetFeatureTags(extended_face.get());
  ASSERT_FALSE(feature_tags.contains(kVrt3));

  static constexpr uint32_t chunk2_gid = 816;
  static constexpr uint32_t chunk4_gid = 800;
  static constexpr uint32_t chunk5_gid = 989;
  static constexpr uint32_t chunk6_gid = 932;
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(extended_face.get(), chunk5_gid)->empty());

  // Ext 2 - extend to {kVrt3}
  extended = ExtendWithDesignSpace(encoder, *encoded, {chunk2_cp}, {kVrt3}, {});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  feature_tags = FontHelper::GetFeatureTags(extended_face.get());
  ASSERT_TRUE(feature_tags.contains(kVrt3));
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(extended_face.get(), chunk4_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk5_gid)->empty());
  ASSERT_TRUE(FontHelper::GlyfData(extended_face.get(), chunk6_gid)->empty());

  // Ext 3 - extend to chunk4_cp + kVrt3
  extended = ExtendWithDesignSpace(encoder, *encoded, {chunk2_cp, chunk4_cp},
                                   {kVrt3}, {});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk4_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk5_gid)->empty());
  ASSERT_FALSE(FontHelper::GlyfData(extended_face.get(), chunk6_gid)->empty());
}

TEST_F(IntegrationTest, MixedMode_LocaLenChange) {
  Encoder encoder;
  auto sc = InitEncoderForMixedMode(encoder);
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
  auto encoded_face = encoded->face();

  auto codepoints = FontHelper::ToCodepointsSet(encoded_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_FALSE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  // ### Phase 1 ###
  auto extended = Extend(encoder, *encoded, {chunk3_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  uint32_t gid_count_1 = hb_face_get_glyph_count(encoded_face.get());
  uint32_t gid_count_2 = hb_face_get_glyph_count(extended_face.get());

  // ### Phase 2 ###
  extended = Extend(encoder, *encoded, {chunk2_cp, chunk3_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  uint32_t gid_count_3 = hb_face_get_glyph_count(extended_face.get());

  // ### Checks ###

  // To avoid loca len change the encoder ensures that a full len
  // loca exists in the base font. So gid count should be consistent
  // at each point
  ASSERT_EQ(gid_count_1, gid_count_2);
  ASSERT_EQ(gid_count_2, gid_count_3);

  codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_FALSE(codepoints.contains(chunk1_cp));
  ASSERT_TRUE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_FALSE(codepoints.contains(chunk4_cp));

  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk0_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(extended_face.get(), chunk1_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk3_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(extended_face.get(), chunk4_gid)->empty());
  ASSERT_TRUE(
      !FontHelper::GlyfData(extended_face.get(), gid_count_3 - 1)->empty());
}

TEST_F(IntegrationTest, MixedMode_Complex) {
  Encoder encoder;
  auto sc = InitEncoderForMixedMode(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0}, {1, 2}, {3, 4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({1, 2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto encoded_face = encoded->face();

  // Phase 1
  auto extended = Extend(encoder, *encoded, {chunk1_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  // Phase 2
  extended = Extend(encoder, *extended, {chunk1_cp, chunk3_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  // Check the results
  auto codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_TRUE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));

  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk0_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk1_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(extended_face.get(), chunk2_gid)->empty());
  ASSERT_TRUE(!FontHelper::GlyfData(extended_face.get(), chunk3_gid)->empty());
  ASSERT_FALSE(!FontHelper::GlyfData(extended_face.get(), chunk4_gid)->empty());
}

TEST_F(IntegrationTest, MixedMode_SequentialDependentPatches) {
  Encoder encoder;
  auto sc = InitEncoderForMixedMode(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3}, {4}}
  sc = encoder.SetBaseSubsetFromIftbPatches({1});
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({2}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({3}));
  sc.Update(encoder.AddExtensionSubsetOfIftbPatches({4}));
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto encoded_face = encoded->face();

  auto extended = Extend(encoder, *encoded, {chunk3_cp, chunk4_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  auto codepoints = FontHelper::ToCodepointsSet(extended_face.get());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));
}

/*
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
  sc = AddPatches(*client, encoder);
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
  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  ASSERT_TRUE(GvarHasLongOffsets(client->GetFontData()));
  hb_face_unique_ptr face = client->GetFontData().face();
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk0_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk1_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(face.get(), chunk2_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(face.get(), chunk3_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(face.get(), chunk4_gid)->size(), 0);

  patches = client->PatchesNeeded();
  expected_patches = {"vf-0x03", "vf-0x04"};
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  face = client->GetFontData().face();
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk0_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk1_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(face.get(), chunk2_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk3_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk4_gid)->size(), 0);
}

TEST_F(IntegrationTest, MixedMode_DesignSpaceAugmentation_DropsUnusedPatches) {
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

  // Phase 1
  client->AddDesiredCodepoints({chunk3_cp, chunk4_cp});
  sc = client->AddDesiredDesignSpace(kWght, 100, 900);
  ASSERT_TRUE(sc.ok()) << sc;
  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  auto patches = client->PatchesNeeded();
  flat_hash_set<std::string> expected_patches = {"0x03", "0x04", "0x06"};
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // Phase 2
  patches = client->PatchesNeeded();
  expected_patches = {"0x0d"};
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // Phase 3
  patches = client->PatchesNeeded();
  expected_patches = {"vf-0x03", "vf-0x04"};
  ASSERT_EQ(patches, expected_patches);
  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Checks

  auto face = client->GetFontData().face();
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk0_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk1_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(face.get(), chunk2_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk3_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(face.get(), chunk4_gid)->size(), 0);
}
*/

}  // namespace ift
