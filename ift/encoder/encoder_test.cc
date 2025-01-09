#include "ift/encoder/encoder.h"

#include <stdlib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/axis_range.h"
#include "common/binary_patch.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "gtest/gtest.h"
#include "ift/client/fontations_client.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StrCat;
using absl::string_view;
using common::AxisRange;
using common::BinaryPatch;
using common::BrotliBinaryPatch;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using ift::client::ToGraph;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::GLYPH_KEYED;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::TABLE_KEYED_FULL;
using ift::proto::TABLE_KEYED_PARTIAL;

namespace ift::encoder {

typedef btree_map<std::string, btree_set<std::string>> graph;
constexpr hb_tag_t kWght = HB_TAG('w', 'g', 'h', 't');
constexpr hb_tag_t kWdth = HB_TAG('w', 'd', 't', 'h');

class EncoderTest : public ::testing::Test {
 protected:
  EncoderTest() {
    font = from_file("common/testdata/Roboto-Regular.abcd.ttf");
    full_font = from_file("common/testdata/Roboto-Regular.ttf");
    woff2_font = from_file("common/testdata/Roboto-Regular.abcd.woff2");
    vf_font = from_file("common/testdata/Roboto[wdth,wght].ttf");
    noto_sans_jp = from_file("ift/testdata/NotoSansJP-Regular.subset.ttf");

    chunk1 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk1.br");
    chunk2 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk2.br");
    chunk3 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk3.br");
    chunk4 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk4.br");
  }

  FontData font;
  FontData full_font;
  FontData woff2_font;
  FontData vf_font;
  FontData noto_sans_jp;
  FontData chunk1;
  FontData chunk2;
  FontData chunk3;
  FontData chunk4;

  uint32_t chunk0_cp = 0x47;
  uint32_t chunk1_cp = 0xb7;
  uint32_t chunk2_cp = 0xb2;
  uint32_t chunk3_cp = 0xeb;
  uint32_t chunk4_cp = 0xa8;

  FontData from_file(const char* filename) {
    hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
    if (!blob) {
      assert(false);
    }
    FontData result(blob);
    hb_blob_destroy(blob);
    return result;
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

  std::string GetVarInfo(const FontData& font_data) {
    auto face = font_data.face();
    constexpr uint32_t max_axes = 5;
    hb_ot_var_axis_info_t info[max_axes];

    uint32_t count = max_axes;
    hb_ot_var_get_axis_infos(face.get(), 0, &count, info);

    std::string result = "";
    bool first = true;
    for (uint32_t i = 0; i < count; i++) {
      std::string tag = FontHelper::ToString(info[i].tag);
      float min = info[i].min_value;
      float max = info[i].max_value;
      if (!first) {
        result = StrCat(result, ";");
      }
      first = false;
      result = StrCat(result, tag, "[", min, ",", max, "]");
    }

    return result;
  }
};

TEST_F(EncoderTest, OutgoingEdges) {
  Encoder encoder;
  encoder.AddExtensionSubset({1, 2});
  encoder.AddExtensionSubset({3, 4});
  encoder.AddExtensionSubset({5, 6});
  encoder.AddExtensionSubset({7, 8});

  Encoder::SubsetDefinition s1{1, 2};
  Encoder::SubsetDefinition s2{3, 4};
  Encoder::SubsetDefinition s3{5, 6};
  Encoder::SubsetDefinition s4{7, 8};

  auto combos = encoder.OutgoingEdges(s2, 1);
  std::vector<Encoder::SubsetDefinition> expected = {s1, s3, s4};
  ASSERT_EQ(combos, expected);

  combos = encoder.OutgoingEdges({1}, 1);
  expected = {{2}, s2, s3, s4};
  ASSERT_EQ(combos, expected);

  combos = encoder.OutgoingEdges(s1, 2);
  expected = {// l1
              {3, 4},
              {5, 6},
              {7, 8},

              // l2
              {3, 4, 5, 6},
              {3, 4, 7, 8},
              {5, 6, 7, 8}};
  ASSERT_EQ(combos, expected);

  combos = encoder.OutgoingEdges(s1, 3);
  expected = {// l1
              {3, 4},
              {5, 6},
              {7, 8},

              // l2
              {3, 4, 5, 6},
              {3, 4, 7, 8},
              {5, 6, 7, 8},

              // l3
              {3, 4, 5, 6, 7, 8}};
  ASSERT_EQ(combos, expected);

  combos = encoder.OutgoingEdges({1, 3, 5, 7}, 3);
  expected = {// l1
              {2},
              {4},
              {6},
              {8},

              // l2
              {2, 4},
              {2, 6},
              {2, 8},
              {4, 6},
              {4, 8},
              {6, 8},

              // l3
              {2, 4, 6},
              {2, 4, 8},
              {2, 6, 8},
              {4, 6, 8}};
  ASSERT_EQ(combos, expected);
}

TEST_F(EncoderTest, OutgoingEdges_DesignSpace_PointToRange) {
  Encoder::SubsetDefinition base{1, 2};
  base.design_space[kWght] = AxisRange::Point(300);

  Encoder encoder;
  encoder.AddExtensionSubset({3, 4});
  encoder.AddOptionalDesignSpace({{kWght, *AxisRange::Range(300, 400)}});

  Encoder::SubsetDefinition s1{3, 4};

  Encoder::SubsetDefinition s2{};
  s2.design_space[kWght] = *AxisRange::Range(300, 400);

  Encoder::SubsetDefinition s3{3, 4};
  s3.design_space[kWght] = *AxisRange::Range(300, 400);

  auto combos = encoder.OutgoingEdges(base, 2);
  std::vector<Encoder::SubsetDefinition> expected = {s1, s2, s3};
  ASSERT_EQ(combos, expected);
}

TEST_F(EncoderTest, OutgoingEdges_DesignSpace_AddAxis_1) {
  Encoder::SubsetDefinition base{1, 2};
  base.design_space[kWght] = *AxisRange::Range(200, 500);

  Encoder encoder;
  encoder.AddExtensionSubset({3, 4});
  encoder.AddOptionalDesignSpace({{kWdth, *AxisRange::Range(300, 400)}});

  Encoder::SubsetDefinition s1{3, 4};

  Encoder::SubsetDefinition s2{};
  s2.design_space[kWdth] = *AxisRange::Range(300, 400);

  Encoder::SubsetDefinition s3{3, 4};
  s3.design_space[kWdth] = *AxisRange::Range(300, 400);

  auto combos = encoder.OutgoingEdges(base, 2);
  std::vector<Encoder::SubsetDefinition> expected = {s1, s2, s3};
  ASSERT_EQ(combos, expected);
}

TEST_F(EncoderTest, OutgoingEdges_DesignSpace_AddAxis_OverlappingAxisRange) {
  Encoder::SubsetDefinition base{1, 2};
  base.design_space[kWght] = *AxisRange::Range(200, 500);

  Encoder encoder;
  encoder.AddExtensionSubset({3, 4});
  encoder.AddOptionalDesignSpace({
      {kWght, *AxisRange::Range(300, 700)},
      {kWdth, *AxisRange::Range(300, 400)},
  });

  Encoder::SubsetDefinition s1{3, 4};

  Encoder::SubsetDefinition s2{};
  // TODO(garretrieger): since the current subtract implementation is limited
  //   we don't support partially subtracting a range. Once support is
  //   available this case can be updated to check wght range is partially
  //   subtracted instead of being ignored.
  s2.design_space[kWdth] = *AxisRange::Range(300, 400);

  Encoder::SubsetDefinition s3{3, 4};
  s3.design_space[kWdth] = *AxisRange::Range(300, 400);

  auto combos = encoder.OutgoingEdges(base, 2);
  std::vector<Encoder::SubsetDefinition> expected = {s1, s2, s3};
  ASSERT_EQ(combos, expected);
}

// TODO(garretrieger): Once the union implementation is updated to
//  support unioning the same axis add tests for that.

TEST_F(EncoderTest, OutgoingEdges_DesignSpace_AddAxis_MergeSpace) {
  Encoder::SubsetDefinition base{1, 2};
  base.design_space[kWght] = AxisRange::Point(300);
  base.design_space[kWdth] = AxisRange::Point(75);

  Encoder encoder;
  encoder.AddOptionalDesignSpace({
      {kWght, *AxisRange::Range(300, 700)},
  });
  encoder.AddOptionalDesignSpace({
      {kWdth, *AxisRange::Range(50, 100)},
  });

  Encoder::SubsetDefinition s1{};
  s1.design_space[kWght] = *AxisRange::Range(300, 700);

  Encoder::SubsetDefinition s2{};
  s2.design_space[kWdth] = *AxisRange::Range(50, 100);

  Encoder::SubsetDefinition s3{};
  s3.design_space[kWght] = *AxisRange::Range(300, 700);
  s3.design_space[kWdth] = *AxisRange::Range(50, 100);

  auto combos = encoder.OutgoingEdges(base, 2);
  std::vector<Encoder::SubsetDefinition> expected = {s1, s2, s3};
  ASSERT_EQ(combos, expected);
}

TEST_F(EncoderTest, MissingFace) {
  Encoder encoder;
  auto s1 = encoder.AddExistingIftbPatch(1, chunk1);
  ASSERT_TRUE(absl::IsFailedPrecondition(s1)) << s1;

  auto s2 = encoder.SetBaseSubsetFromIftbPatches({});
  ASSERT_TRUE(absl::IsFailedPrecondition(s2)) << s2;

  auto s3 = encoder.Encode();
  ASSERT_TRUE(absl::IsFailedPrecondition(s3.status())) << s3.status();
}

TEST_F(EncoderTest, IftbGidsNotInFace) {
  Encoder encoder;
  {
    hb_face_t* face = font.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }

  auto s = encoder.AddExistingIftbPatch(1, chunk1);
  ASSERT_TRUE(absl::IsInvalidArgument(s)) << s;
}

TEST_F(EncoderTest, InvalidIftbIds) {
  Encoder encoder;
  {
    hb_face_t* face = noto_sans_jp.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }

  auto s = encoder.AddExistingIftbPatch(1, chunk1);
  ASSERT_TRUE(s.ok()) << s;

  s = encoder.AddExtensionSubsetOfIftbPatches({2});
  ASSERT_TRUE(absl::IsInvalidArgument(s)) << s;

  s = encoder.SetBaseSubsetFromIftbPatches({2});
  ASSERT_TRUE(absl::IsInvalidArgument(s)) << s;
}

TEST_F(EncoderTest, DontClobberBaseSubset) {
  Encoder encoder;
  {
    hb_face_t* face = noto_sans_jp.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }

  auto s = encoder.AddExistingIftbPatch(1, chunk1);
  ASSERT_TRUE(s.ok()) << s;

  s = encoder.SetBaseSubsetFromIftbPatches({});
  ASSERT_TRUE(s.ok()) << s;

  s = encoder.SetBaseSubset({1});
  ASSERT_TRUE(absl::IsFailedPrecondition(s)) << s;

  s = encoder.SetBaseSubsetFromIftbPatches({});
  ASSERT_TRUE(absl::IsFailedPrecondition(s)) << s;
}

TEST_F(EncoderTest, Encode_OneSubset) {
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);

  auto s = encoder.SetBaseSubset({'a', 'd'});
  ASSERT_TRUE(s.ok()) << s;
  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{{"ad", {}}};
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_TwoSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b', 'c'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);
  auto s = encoder.SetBaseSubset({'a', 'd'});
  ASSERT_TRUE(s.ok()) << s;
  encoder.AddExtensionSubset(s1);

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{{"ad", {"abcd"}}, {"abcd", {}}};
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_TwoSubsetsAndOptionalFeature) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'B', 'C'};
  Encoder encoder;
  hb_face_t* face = full_font.reference_face();
  encoder.SetFace(face);
  auto s = encoder.SetBaseSubset({'A', 'D'});
  ASSERT_TRUE(s.ok()) << s;
  encoder.AddExtensionSubset(s1);
  encoder.AddOptionalFeatureGroup({HB_TAG('c', '2', 's', 'c')});

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{
      {"AD", {"ABCD", "AD|c2sc"}},
      {"AD|c2sc", {"ABCD|c2sc"}},
      {"ABCD", {"ABCD|c2sc"}},
      {"ABCD|c2sc", {}},
  };
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_ThreeSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);
  auto s = encoder.SetBaseSubset({'a'});
  ASSERT_TRUE(s.ok()) << s;
  encoder.AddExtensionSubset(s1);
  encoder.AddExtensionSubset(s2);

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(encoder.Patches().size(), 4);

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{
      {"a", {"ab", "ac"}},
      {"ab", {"abc"}},
      {"ac", {"abc"}},
      {"abc", {}},
  };
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_ThreeSubsets_VF) {
  Encoder encoder;
  hb_face_t* face = vf_font.reference_face();
  encoder.SetFace(face);

  Encoder::SubsetDefinition base_def{'a'};
  base_def.design_space[kWdth] = AxisRange::Point(100.0f);
  auto s = encoder.SetBaseSubsetFromDef(base_def);
  ASSERT_TRUE(s.ok()) << s;

  encoder.AddExtensionSubset({'b'});
  encoder.AddOptionalDesignSpace({{kWdth, *AxisRange::Range(75.0f, 100.0f)}});

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(encoder.Patches().size(), 4);

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{
      {"a|wght[100..900]",
       {"ab|wght[100..900]", "a|wght[100..900],wdth[75..100]"}},
      {"ab|wght[100..900]", {"ab|wght[100..900],wdth[75..100]"}},
      {"a|wght[100..900],wdth[75..100]", {"ab|wght[100..900],wdth[75..100]"}},
      {"ab|wght[100..900],wdth[75..100]", {}},
  };
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_ThreeSubsets_Mixed) {
  Encoder encoder;
  {
    hb_face_t* face = noto_sans_jp.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }

  auto s = encoder.AddExistingIftbPatch(1, chunk1);
  s.Update(encoder.AddExistingIftbPatch(2, chunk2));
  s.Update(encoder.AddExistingIftbPatch(3, chunk3));
  s.Update(encoder.AddExistingIftbPatch(4, chunk4));
  ASSERT_TRUE(s.ok()) << s;

  s.Update(encoder.SetBaseSubsetFromIftbPatches({1, 2}));
  s.Update(encoder.AddExtensionSubsetOfIftbPatches({3, 4}));
  ASSERT_TRUE(s.ok()) << s;

  auto base = encoder.Encode();

  ASSERT_TRUE(base.ok()) << base.status();
  auto cps = ToCodepointsSet(*base);
  ASSERT_TRUE(cps.contains(chunk0_cp));
  ASSERT_TRUE(cps.contains(chunk1_cp));
  ASSERT_TRUE(cps.contains(chunk2_cp));
  ASSERT_FALSE(cps.contains(chunk3_cp));
  ASSERT_FALSE(cps.contains(chunk4_cp));

  ASSERT_EQ(encoder.Patches().size(), 3);

  // TODO(garretrieger): check the iftb entries in the base and check
  //  they are unmodified in derived fonts.
  // TODO(garretrieger): apply a iftb patch and then check that you
  //  can still form the graph with derived fonts containing the
  //  modified glyf, loca, and IFT table.

  {
    hb_face_t* face = base->reference_face();
    auto iftx_data = FontHelper::TableData(face, HB_TAG('I', 'F', 'T', 'X'));
    ASSERT_FALSE(iftx_data.empty());
    hb_face_destroy(face);
  }

  // expected patches:
  // - chunk 3 (iftb)
  // - chunk 4 (iftb)
  // - shared brotli to (chunk 3 + 4)
  // TODO XXXXX Check graph instead
}

TEST_F(EncoderTest, Encode_ThreeSubsets_Mixed_WithFeatureMappings) {
  Encoder encoder;
  {
    hb_face_t* face = noto_sans_jp.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }

  auto s = encoder.AddExistingIftbPatch(1, chunk1);
  s.Update(encoder.AddExistingIftbPatch(2, chunk2));
  s.Update(encoder.AddExistingIftbPatch(3, chunk3));
  s.Update(encoder.AddExistingIftbPatch(4, chunk4));
  s.Update(
      encoder.AddIftbFeatureSpecificPatch(3, 4, HB_TAG('c', 'c', 'm', 'p')));
  ASSERT_TRUE(s.ok()) << s;

  // Partitions {1}, {2, 3, 4}, +ccmp
  s.Update(encoder.SetBaseSubsetFromIftbPatches({1}));
  s.Update(encoder.AddExtensionSubsetOfIftbPatches({2, 3, 4}));
  encoder.AddOptionalFeatureGroup({HB_TAG('c', 'c', 'm', 'p')});
  ASSERT_TRUE(s.ok()) << s;

  auto base = encoder.Encode();
  ASSERT_TRUE(base.ok()) << base.status();

  ASSERT_EQ(encoder.Patches().size(), 7);

  // expected patches:
  // - chunk 2 (iftb)
  // - chunk 3 (iftb)
  // - chunk 4 (iftb), triggered by ccmap + chunk 3
  // - shared brotli patches...
  // TODO XXXXX Check graph instead
}

TEST_F(EncoderTest, Encode_FourSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  absl::flat_hash_set<hb_codepoint_t> s3 = {'d'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);
  auto s = encoder.SetBaseSubset({'a'});
  ASSERT_TRUE(s.ok()) << s;
  encoder.AddExtensionSubset(s1);
  encoder.AddExtensionSubset(s2);
  encoder.AddExtensionSubset(s3);

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(encoder.Patches().size(), 12);

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{
      {"a", {"ab", "ac", "ad"}}, {"ab", {"abc", "abd"}}, {"ac", {"abc", "acd"}},
      {"ad", {"abd", "acd"}},    {"abc", {"abcd"}},      {"abd", {"abcd"}},
      {"acd", {"abcd"}},         {"abcd", {}},
  };
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_FourSubsets_WithJumpAhead) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  absl::flat_hash_set<hb_codepoint_t> s3 = {'d'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);
  auto s = encoder.SetBaseSubset({'a'});
  ASSERT_TRUE(s.ok()) << s;
  encoder.AddExtensionSubset(s1);
  encoder.AddExtensionSubset(s2);
  encoder.AddExtensionSubset(s3);
  encoder.SetJumpAhead(2);

  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(encoder.Patches().size(), 18);

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{
      {"a", {"ab", "ac", "ad", "abc", "abd", "acd"}},
      {"ab", {"abc", "abd", "abcd"}},
      {"ac", {"abc", "acd", "abcd"}},
      {"ad", {"abd", "acd", "abcd"}},
      {"abc", {"abcd"}},
      {"abd", {"abcd"}},
      {"acd", {"abcd"}},
      {"abcd", {}},
  };
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, RoundTripWoff2) {
  auto ttf = Encoder::RoundTripWoff2(font.str());
  ASSERT_TRUE(ttf.ok()) << ttf.status();

  ASSERT_GT(ttf->size(), 4);
  uint8_t true_type_tag[] = {0, 1, 0, 0};
  ASSERT_EQ(true_type_tag, Span<const uint8_t>((const uint8_t*)ttf->data(), 4));
}

TEST_F(EncoderTest, RoundTripWoff2_Fails) {
  auto ttf = Encoder::RoundTripWoff2(woff2_font.str());
  ASSERT_TRUE(absl::IsInternal(ttf.status())) << ttf.status();
}

}  // namespace ift::encoder
