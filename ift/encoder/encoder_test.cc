#include "ift/encoder/encoder.h"

#include <cstdint>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/binary_patch.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "gtest/gtest.h"
#include "ift/per_table_brotli_binary_patch.h"
#include "ift/proto/ift_table.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StrCat;
using absl::string_view;
using common::BinaryPatch;
using common::BrotliBinaryPatch;
using common::FontData;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;

namespace ift::encoder {

typedef btree_map<std::string, btree_set<std::string>> graph;

class EncoderTest : public ::testing::Test {
 protected:
  EncoderTest() {
    font = from_file("patch_subset/testdata/Roboto-Regular.abcd.ttf");
    full_font = from_file("patch_subset/testdata/Roboto-Regular.ttf");
    woff2_font = from_file("patch_subset/testdata/Roboto-Regular.abcd.woff2");
    noto_sans_jp = from_file("ift/testdata/NotoSansJP-Regular.subset.ttf");

    chunk1 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk1.br");
    chunk2 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk2.br");
    chunk3 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk3.br");
    chunk4 = from_file("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk4.br");
  }

  FontData font;
  FontData full_font;
  FontData woff2_font;
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
    hb_blob_t* blob = hb_blob_create_from_file(filename);
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

  std::string ToNodeName(const FontData& font_data) {
    std::string result;
    for (uint32_t cp : ToCodepointsSet(font_data)) {
      result.push_back(cp);
    }

    auto face = font_data.face();
    auto feature_tags = FontHelper::GetNonDefaultFeatureTags(face.get());
    if (feature_tags.empty()) {
      return result;
    }

    result += "|";

    bool first = true;
    auto string_tags = FontHelper::ToStrings(feature_tags);
    for (const std::string& tag : string_tags) {
      if (!first) {
        result += ",";
      }
      result += tag;
      first = true;
    }

    return result;
  }

  Status ToGraph(const Encoder& encoder, const FontData& base, graph& out) {
    auto ift_table = IFTTable::FromFont(base);

    btree_set<uint32_t> base_set = ToCodepointsSet(base);
    std::string node = ToNodeName(base);
    auto it = out.find(node);
    if (it != out.end()) {
      return absl::OkStatus();
    }

    out[node] = {};
    if (!ift_table.ok()) {
      // No 'IFT ' table means this is a leaf.
      return absl::OkStatus();
    }

    PatchEncoding encoding = DEFAULT_ENCODING;
    flat_hash_set<uint32_t> subsets;
    for (const auto& e : ift_table->GetPatchMap().GetEntries()) {
      if (e.encoding != SHARED_BROTLI_ENCODING &&
          e.encoding != PER_TABLE_SHARED_BROTLI_ENCODING) {
        // Ignore IFTB patches which don't form the graph.
        continue;
      }
      if (encoding == DEFAULT_ENCODING) {
        encoding = e.encoding;
      }
      if (encoding != e.encoding) {
        return absl::InternalError("Inconsistent encodings.");
      }
      subsets.insert(e.patch_index);
    }

    PerTableBrotliBinaryPatch per_table_patcher;
    BrotliBinaryPatch brotli_patcher;
    const BinaryPatch* patcher = (encoding == SHARED_BROTLI_ENCODING)
                                     ? (BinaryPatch*)&brotli_patcher
                                     : (BinaryPatch*)&per_table_patcher;
    for (uint32_t id : subsets) {
      auto it = encoder.Patches().find(id);
      if (it == encoder.Patches().end()) {
        return absl::InternalError("patch not found.");
      }

      const FontData& patch = it->second;
      FontData derived;
      auto sc = patcher->Patch(base, patch, &derived);
      if (!sc.ok()) {
        return sc;
      }
      std::string child = ToNodeName(derived);
      out[node].insert(child);

      sc = ToGraph(encoder, derived, out);
      if (!sc.ok()) {
        return sc;
      }
    }
    return absl::OkStatus();
  }
};

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
  ASSERT_EQ(ToNodeName(font), "abcd");

  Encoder encoder;
  hb_face_t* face = font.reference_face();
  encoder.SetFace(face);

  auto s = encoder.SetBaseSubset({'a', 'd'});
  ASSERT_TRUE(s.ok()) << s;
  auto base = encoder.Encode();
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToNodeName(*base), "ad");
  ASSERT_TRUE(encoder.Patches().empty());

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
  ASSERT_EQ(ToNodeName(*base), "ad");
  ASSERT_FALSE(encoder.Patches().empty());

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
  ASSERT_EQ(ToNodeName(*base), "AD");
  ASSERT_FALSE(encoder.Patches().empty());

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
  ASSERT_EQ(ToNodeName(*base), "a");
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

  ASSERT_EQ(encoder.Patches().size(), 1);

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

  auto ift_table = IFTTable::FromFont(*base);
  ASSERT_TRUE(ift_table.ok()) << ift_table.status();

  // expected patches:
  // - chunk 3 (iftb)
  // - chunk 4 (iftb)
  // - shared brotli to (chunk 3 + 4)
  ASSERT_EQ(ift_table->GetPatchMap().GetEntries().size(), 3);

  const auto& entry0 = ift_table->GetPatchMap().GetEntries()[0];
  ASSERT_EQ(entry0.patch_index, 3);
  ASSERT_FALSE(entry0.coverage.codepoints.contains(chunk0_cp));
  ASSERT_FALSE(entry0.coverage.codepoints.contains(chunk1_cp));
  ASSERT_FALSE(entry0.coverage.codepoints.contains(chunk2_cp));
  ASSERT_TRUE(entry0.coverage.codepoints.contains(chunk3_cp));
  ASSERT_FALSE(entry0.coverage.codepoints.contains(chunk4_cp));

  const auto& entry1 = ift_table->GetPatchMap().GetEntries()[1];
  ASSERT_EQ(entry1.patch_index, 4);
  ASSERT_FALSE(entry1.coverage.codepoints.contains(chunk0_cp));
  ASSERT_FALSE(entry1.coverage.codepoints.contains(chunk1_cp));
  ASSERT_FALSE(entry1.coverage.codepoints.contains(chunk2_cp));
  ASSERT_FALSE(entry1.coverage.codepoints.contains(chunk3_cp));
  ASSERT_TRUE(entry1.coverage.codepoints.contains(chunk4_cp));

  const auto& entry2 = ift_table->GetPatchMap().GetEntries()[2];
  ASSERT_EQ(entry2.patch_index, 5);
  ASSERT_FALSE(entry2.coverage.codepoints.contains(chunk0_cp));
  ASSERT_FALSE(entry2.coverage.codepoints.contains(chunk1_cp));
  ASSERT_FALSE(entry2.coverage.codepoints.contains(chunk2_cp));
  ASSERT_TRUE(entry2.coverage.codepoints.contains(chunk3_cp));
  ASSERT_TRUE(entry2.coverage.codepoints.contains(chunk4_cp));
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
  ASSERT_EQ(ToNodeName(*base), "a");
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

TEST_F(EncoderTest, EncodeWoff2) {
  auto woff2 = Encoder::EncodeWoff2(font.str());
  ASSERT_TRUE(woff2.ok()) << woff2.status();

  ASSERT_GT(woff2->size(), 48);
  ASSERT_EQ("wOF2", string_view(woff2->data(), 4));
  ASSERT_LT(woff2->size(), font.size());
}

TEST_F(EncoderTest, EncodeWoff2_NoGlyfTransform) {
  auto woff2 = Encoder::EncodeWoff2(font.str(), true);
  auto woff2_no_transform = Encoder::EncodeWoff2(font.str(), false);
  ASSERT_TRUE(woff2.ok()) << woff2.status();
  ASSERT_TRUE(woff2_no_transform.ok()) << woff2_no_transform.status();

  ASSERT_GT(woff2->size(), 48);
  ASSERT_GT(woff2_no_transform->size(), 48);
  ASSERT_EQ("wOF2", string_view(woff2->data(), 4));
  ASSERT_EQ("wOF2", string_view(woff2_no_transform->data(), 4));
  ASSERT_LT(woff2->size(), font.size());
  ASSERT_LT(woff2_no_transform->size(), font.size());
  ASSERT_NE(woff2_no_transform->size(), woff2->size());
}

TEST_F(EncoderTest, EncodeWoff2_Fails) {
  auto woff2 = Encoder::EncodeWoff2(woff2_font.str());
  ASSERT_TRUE(absl::IsInternal(woff2.status())) << woff2.status();
}

TEST_F(EncoderTest, DecodeWoff2) {
  auto font = Encoder::DecodeWoff2(woff2_font.str());
  ASSERT_TRUE(font.ok()) << font.status();

  ASSERT_GT(font->size(), woff2_font.size());
  uint8_t true_type_tag[] = {0, 1, 0, 0};
  ASSERT_EQ(true_type_tag,
            Span<const uint8_t>((const uint8_t*)font->data(), 4));
}

TEST_F(EncoderTest, DecodeWoff2_Fails) {
  auto ttf = Encoder::DecodeWoff2(font.str());
  ASSERT_TRUE(absl::IsInternal(ttf.status())) << ttf.status();
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
