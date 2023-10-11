#include "ift/encoder/encoder.h"

#include <cstdint>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"

using absl::btree_map;
using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StrCat;
using ift::proto::IFTTable;
using patch_subset::BrotliBinaryPatch;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;

namespace ift::encoder {

typedef btree_map<std::string, btree_set<std::string>> graph;

class EncoderTest : public ::testing::Test {
 protected:
  EncoderTest() {
    font = from_file("patch_subset/testdata/Roboto-Regular.abcd.ttf");
  }

  FontData font;

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

    if (!ift_table.ok()) {
      // No 'IFT ' table means this is a leaf.
      out[node] = {};
      return absl::OkStatus();
    }

    flat_hash_map<uint32_t, btree_set<uint32_t>> subsets;
    for (const auto& p : ift_table->GetPatchMap()) {
      uint32_t cp = p.first;
      uint32_t id = p.second.first;
      subsets[id].insert(cp);
    }

    // add the base set to all entries.
    for (auto& p : subsets) {
      for (uint32_t cp : base_set) {
        p.second.insert(cp);
      }
    }

    BrotliBinaryPatch patcher;
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
      auto sc = patcher.Patch(base, patch, &derived);
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
  auto base = encoder.Encode(font.reference_face(), {'a', 'd'}, {&s1});
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
  auto base = encoder.Encode(font.reference_face(), {'a'}, {&s1, &s2});
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

TEST_F(EncoderTest, Encode_FourSubsets) {
  absl::flat_hash_set<hb_codepoint_t> s1 = {'b'};
  absl::flat_hash_set<hb_codepoint_t> s2 = {'c'};
  absl::flat_hash_set<hb_codepoint_t> s3 = {'d'};
  Encoder encoder;
  hb_face_t* face = font.reference_face();
  auto base = encoder.Encode(font.reference_face(), {'a'}, {&s1, &s2, &s3});
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

}  // namespace ift::encoder