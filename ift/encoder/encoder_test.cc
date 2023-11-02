#include "ift/encoder/encoder.h"

#include <cstdint>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "ift/per_table_brotli_binary_patch.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StrCat;
using absl::string_view;
using common::FontHelper;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BinaryPatch;
using patch_subset::BrotliBinaryPatch;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;

namespace ift::encoder {

typedef btree_map<std::string, btree_set<std::string>> graph;

class EncoderTest : public ::testing::Test {
 protected:
  EncoderTest() {
    font = from_file("patch_subset/testdata/Roboto-Regular.abcd.ttf");
    woff2_font = from_file("patch_subset/testdata/Roboto-Regular.abcd.woff2");
  }

  FontData font;
  FontData woff2_font;

  FontData from_file(const char* filename) {
    hb_blob_t* blob = hb_blob_create_from_file(filename);
    FontData result(blob);
    hb_blob_destroy(blob);
    return result;
  }

  btree_set<uint32_t> ToCodepointsSet(const FontData& font_data) {
    hb_face_t* face = font_data.reference_face();

    hb_set_unique_ptr codepoints = patch_subset::make_hb_set();
    hb_face_collect_unicodes(face, codepoints.get());
    hb_face_destroy(face);

    btree_set<uint32_t> result;
    hb_codepoint_t cp = HB_SET_VALUE_INVALID;
    while (hb_set_next(codepoints.get(), &cp)) {
      result.insert(cp);
    }

    return result;
  }

  std::string ToCodepoints(const FontData& font_data) {
    std::string result;
    for (uint32_t cp : ToCodepointsSet(font_data)) {
      result.push_back(cp);
    }

    return result;
  }

  Status ToGraph(const Encoder& encoder, const FontData& base, graph& out) {
    auto ift_table = IFTTable::FromFont(base);

    btree_set<uint32_t> base_set = ToCodepointsSet(base);
    std::string node = ToCodepoints(base);
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
    flat_hash_map<uint32_t, btree_set<uint32_t>> subsets;
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
      uint32_t id = e.patch_index;
      for (uint32_t cp : e.coverage.codepoints) {
        subsets[id].insert(cp);
      }
    }

    // add the base set to all entries.
    for (auto& p : subsets) {
      for (uint32_t cp : base_set) {
        p.second.insert(cp);
      }
    }

    PerTableBrotliBinaryPatch per_table_patcher;
    BrotliBinaryPatch brotli_patcher;
    const BinaryPatch* patcher = (encoding == SHARED_BROTLI_ENCODING)
                                     ? (BinaryPatch*)&brotli_patcher
                                     : (BinaryPatch*)&per_table_patcher;
    for (auto p : subsets) {
      uint32_t id = p.first;
      std::string child;
      for (auto cp : p.second) {
        child.push_back(cp);
      }

      out[node].insert(child);

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

      sc = ToGraph(encoder, derived, out);
      if (!sc.ok()) {
        return sc;
      }
    }
    return absl::OkStatus();
  }
};

TEST_F(EncoderTest, Encode_OneSubset) {
  ASSERT_EQ(ToCodepoints(font), "abcd");

  Encoder encoder;
  hb_face_t* face = font.reference_face();
  auto base = encoder.Encode(face, {'a', 'd'}, {});
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToCodepoints(*base), "ad");
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
  auto base = encoder.Encode(face, {'a', 'd'}, {&s1});
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToCodepoints(*base), "ad");
  ASSERT_FALSE(encoder.Patches().empty());

  graph g;
  auto sc = ToGraph(encoder, *base, g);
  ASSERT_TRUE(sc.ok()) << sc;

  graph expected{{"ad", {"abcd"}}, {"abcd", {}}};
  ASSERT_EQ(g, expected);
}

TEST_F(EncoderTest, Encode_ThreeSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  auto base = encoder.Encode(face, {'a'}, {&s1, &s2});
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToCodepoints(*base), "a");
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
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  Encoder encoder;
  encoder.AddExistingIftbPatch(0, {41});
  encoder.AddExistingIftbPatch(1, {42});
  hb_face_t* face = font.reference_face();
  auto base = encoder.Encode(face, {'a'}, {&s1, &s2});
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToCodepoints(*base), "a");
  ASSERT_EQ(encoder.Patches().size(), 4);

  // TODO(garretrieger): check the iftb entries in the base and check
  //  they are unmodified in derived fonts.
  // TODO(garretrieger): apply a iftb patch and then check that you
  //  can still form the graph with derived fonts containing the
  //  modified glyf, loca, and IFT table.

  face = base->reference_face();
  auto iftx_data = FontHelper::TableData(face, HB_TAG('I', 'F', 'T', 'X'));
  ASSERT_FALSE(iftx_data.empty());
  hb_face_destroy(face);

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

TEST_F(EncoderTest, Encode_FourSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  absl::flat_hash_set<hb_codepoint_t> s3 = {'d'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  auto base = encoder.Encode(face, {'a'}, {&s1, &s2, &s3});
  hb_face_destroy(face);

  ASSERT_TRUE(base.ok()) << base.status();
  ASSERT_EQ(ToCodepoints(*base), "a");
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