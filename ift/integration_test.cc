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
#include "ift/testdata/test_segments.h"

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
using ift::testdata::TestFeatureSegment1;
using ift::testdata::TestFeatureSegment2;
using ift::testdata::TestFeatureSegment3;
using ift::testdata::TestFeatureSegment4;
using ift::testdata::TestFeatureSegment5;
using ift::testdata::TestFeatureSegment6;
using ift::testdata::TestSegment1;
using ift::testdata::TestSegment2;
using ift::testdata::TestSegment3;
using ift::testdata::TestSegment4;
using ift::testdata::TestVfSegment1;
using ift::testdata::TestVfSegment2;
using ift::testdata::TestVfSegment3;
using ift::testdata::TestVfSegment4;
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

    // Noto Sans JP VF
    blob = make_hb_blob(
        hb_blob_create_from_file("ift/testdata/NotoSansJP[wght].subset.ttf"));
    noto_sans_vf_.set(blob.get());

    // Feature Test
    blob = make_hb_blob(hb_blob_create_from_file(
        "ift/testdata/NotoSansJP-Regular.feature-test.ttf"));
    feature_test_.set(blob.get());

    blob = make_hb_blob(
        hb_blob_create_from_file("common/testdata/Roboto[wdth,wght].ttf"));
    roboto_vf_.set(blob.get());
  }

  Status InitEncoderForMixedMode(Encoder& encoder) {
    auto face = noto_sans_jp_.face();
    encoder.SetFace(face.get());

    auto sc = encoder.AddGlyphDataSegment(1, TestSegment1());
    sc.Update(encoder.AddGlyphDataSegment(2, TestSegment2()));
    sc.Update(encoder.AddGlyphDataSegment(3, TestSegment3()));
    sc.Update(encoder.AddGlyphDataSegment(4, TestSegment4()));
    return sc;
  }

  Status InitEncoderForVfMixedMode(Encoder& encoder) {
    auto face = noto_sans_vf_.face();
    encoder.SetFace(face.get());

    auto sc = encoder.AddGlyphDataSegment(1, TestVfSegment1());
    sc.Update(encoder.AddGlyphDataSegment(2, TestVfSegment2()));
    sc.Update(encoder.AddGlyphDataSegment(3, TestVfSegment3()));
    sc.Update(encoder.AddGlyphDataSegment(4, TestVfSegment4()));
    return sc;
  }

  Status InitEncoderForMixedModeFeatureTest(Encoder& encoder) {
    auto face = feature_test_.face();
    encoder.SetFace(face.get());

    auto sc = encoder.AddGlyphDataSegment(1, TestFeatureSegment1());
    sc.Update(encoder.AddGlyphDataSegment(2, TestFeatureSegment2()));
    sc.Update(encoder.AddGlyphDataSegment(3, TestFeatureSegment3()));
    sc.Update(encoder.AddGlyphDataSegment(4, TestFeatureSegment4()));
    sc.Update(encoder.AddGlyphDataSegment(5, TestFeatureSegment5()));
    sc.Update(encoder.AddGlyphDataSegment(6, TestFeatureSegment6()));
    return sc;
  }

  Status InitEncoderForTableKeyed(Encoder& encoder) {
    auto face = noto_sans_jp_.face();
    encoder.SetFace(face.get());
    return absl::OkStatus();
  }

  Status InitEncoderForVf(Encoder& encoder) {
    auto face = roboto_vf_.face();
    encoder.SetFace(face.get());
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
  FontData noto_sans_vf_;

  FontData feature_test_;

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

bool GvarDataMatches(hb_face_t* a, hb_face_t* b, uint32_t codepoint,
                     uint32_t ignore_count) {
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

  auto a_data = FontHelper::GvarData(a, gid_a);
  auto b_data = FontHelper::GvarData(b, gid_b);

  return a_data->substr(ignore_count) == b_data->substr(ignore_count);
}

// TODO(garretrieger): full expansion test.
// TODO(garretrieger): test of a woff2 encoded IFT font.

TEST_F(IntegrationTest, TableKeyedOnly) {
  Encoder encoder;
  auto sc = InitEncoderForTableKeyed(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  sc = encoder.SetBaseSubset({0x41, 0x42, 0x43});
  encoder.AddNonGlyphDataSegment({0x45, 0x46, 0x47});
  encoder.AddNonGlyphDataSegment({0x48, 0x49, 0x4A});
  encoder.AddNonGlyphDataSegment({0x4B, 0x4C, 0x4D});
  encoder.AddNonGlyphDataSegment({0x4E, 0x4F, 0x50});
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
  encoder.AddNonGlyphDataSegment({0x45, 0x46, 0x47});
  encoder.AddNonGlyphDataSegment({0x48, 0x49, 0x4A});
  encoder.AddNonGlyphDataSegment({0x4B, 0x4C, 0x4D});
  encoder.AddNonGlyphDataSegment({0x4E, 0x4F, 0x50});
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
  encoder.AddNonGlyphDataSegment(
      {0x45, 0x46, 0x47, 0x48});  // 0x48 is in two subsets
  encoder.AddNonGlyphDataSegment({0x48, 0x49, 0x4A});
  encoder.AddNonGlyphDataSegment({0x4B, 0x4C, 0x4D});
  encoder.AddNonGlyphDataSegment({0x4E, 0x4F, 0x50});
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

  encoder.AddNonGlyphDataSegment({'d', 'e', 'f'});
  encoder.AddNonGlyphDataSegment({'h', 'i', 'j'});
  encoder.AddDesignSpaceSegment({{kWdth, *AxisRange::Range(75.0f, 100.0f)}});
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

  encoder.AddNonGlyphDataSegment({'d', 'e', 'f'});
  encoder.AddNonGlyphDataSegment({'h', 'i', 'j'});
  encoder.AddDesignSpaceSegment({{kWdth, *AxisRange::Range(75.0f, 100.0f)}});
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
  sc = encoder.SetBaseSubsetFromSegments({1});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3, 4}));
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
  sc = encoder.SetBaseSubsetFromSegments({});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({1}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({4}));
  sc.Update(encoder.AddFeatureDependency(1, 5, kVrt3));
  sc.Update(encoder.AddFeatureDependency(2, 5, kVrt3));
  sc.Update(encoder.AddFeatureDependency(4, 6, kVrt3));
  encoder.AddFeatureGroupSegment({kVrt3});
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
  sc = encoder.SetBaseSubsetFromSegments({});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({1}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({4}));
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
  sc = encoder.SetBaseSubsetFromSegments({});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({1, 2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3, 4}));
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
  sc = encoder.SetBaseSubsetFromSegments({1});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({4}));
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

TEST_F(IntegrationTest, MixedMode_DesignSpaceAugmentation) {
  Encoder encoder;
  auto sc = InitEncoderForVfMixedMode(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3, 4}} + add wght axis
  sc = encoder.SetBaseSubsetFromSegments({1}, {{kWght, AxisRange::Point(100)}});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3, 4}));
  encoder.AddDesignSpaceSegment({{kWght, *AxisRange::Range(100, 900)}});
  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto encoded_face = encoded->face();

  // Phase 1: non VF augmentation.
  auto extended = Extend(encoder, *encoded, {chunk3_cp, chunk4_cp});
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  // Phase 2: VF augmentation.
  extended = ExtendWithDesignSpace(encoder, *encoded, {chunk3_cp, chunk4_cp},
                                   {}, {{kWght, *AxisRange::Range(100, 900)}});
  ASSERT_TRUE(extended.ok()) << extended.status();
  extended_face = extended->face();

  ASSERT_TRUE(GvarHasLongOffsets(*extended));
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk0_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk1_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(extended_face.get(), chunk2_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk3_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk4_gid)->size(), 0);

  auto orig_face = noto_sans_vf_.face();
  // The instancing processes changes some of the flags on the gvar data section
  // so ignore diffs in the first 7 bytes
  ASSERT_TRUE(
      GvarDataMatches(orig_face.get(), extended_face.get(), chunk3_cp, 7));
}

TEST_F(IntegrationTest, MixedMode_DesignSpaceAugmentation_DropsUnusedPatches) {
  Encoder encoder;
  auto sc = InitEncoderForVfMixedMode(encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  // target paritions: {{0, 1}, {2}, {3, 4}} + add wght axis
  sc = encoder.SetBaseSubsetFromSegments({1}, {{kWght, AxisRange::Point(100)}});
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({2}));
  sc.Update(encoder.AddNonGlyphSegmentFromGlyphSegments({3, 4}));
  encoder.AddDesignSpaceSegment({{kWght, *AxisRange::Range(100, 900)}});

  ASSERT_TRUE(sc.ok()) << sc;

  auto encoded = encoder.Encode();
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  auto encoded_face = encoded->face();

  btree_set<std::string> fetched_uris;
  auto extended = ExtendWithDesignSpace(
      encoder, *encoded, {chunk3_cp, chunk4_cp}, {},
      {{kWght, *AxisRange::Range(100, 900)}}, &fetched_uris);

  // correspond to ids 3, 4, 6, d
  btree_set<std::string> expected_uris{"0O.tk",   "1K.tk",   "1_0C.gk",
                                       "1_0G.gk", "2_0C.gk", "2_0G.gk"};

  ASSERT_EQ(fetched_uris, expected_uris);

  // TODO check the patches that were used by looking at ift_extend output
  ASSERT_TRUE(extended.ok()) << extended.status();
  auto extended_face = extended->face();

  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk0_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk1_gid)->size(), 0);
  ASSERT_EQ(FontHelper::GvarData(extended_face.get(), chunk2_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk3_gid)->size(), 0);
  ASSERT_GT(FontHelper::GvarData(extended_face.get(), chunk4_gid)->size(), 0);
}

}  // namespace ift
