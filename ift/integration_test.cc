#include <iterator>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/encoder/encoder.h"
#include "ift/ift_client.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::StrCat;
using ift::IFTClient;
using ift::encoder::Encoder;
using ift::proto::IFTB_ENCODING;
using ift::proto::PatchEncoding;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;

namespace ift {

class IntegrationTest : public ::testing::Test {
 protected:
  IntegrationTest() {
    hb_blob_t* blob =
        hb_blob_create_from_file("ift/testdata/NotoSansJP-Regular.subset.ttf");
    hb_face_t* face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
    noto_sans_jp_.set(face);
    hb_face_destroy(face);

    iftb_patches_.resize(5);
    for (int i = 1; i <= 4; i++) {
      std::string name =
          StrCat("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk", i, ".br");
      blob = hb_blob_create_from_file(name.c_str());
      assert(hb_blob_get_length(blob) > 0);
      iftb_patches_[i].set(blob);
      hb_blob_destroy(blob);
    }
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

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;

  uint32_t chunk0_cp = 0x47;
  uint32_t chunk1_cp = 0xb7;
  uint32_t chunk2_cp = 0xb2;
  uint32_t chunk3_cp = 0xeb;
  uint32_t chunk4_cp = 0xa8;
};

TEST_F(IntegrationTest, MixedMode) {
  Encoder encoder;
  encoder.SetUrlTemplate("$2$1");
  {
    hb_face_t* face = noto_sans_jp_.reference_face();
    encoder.SetFace(face);
    hb_face_destroy(face);
  }
  auto sc = encoder.SetId({0x3c2bfda0, 0x890625c9, 0x40c644de, 0xb1195627});
  ASSERT_TRUE(sc.ok()) << sc;

  for (int i = 1; i <= 4; i++) {
    auto sc = encoder.AddExistingIftbPatch(i, iftb_patches_[i]);
    ASSERT_TRUE(sc.ok()) << sc;
  }

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

  hb_set_unique_ptr input = make_hb_set(2, chunk3_cp, chunk4_cp);
  auto patches = client->PatchUrlsFor(*input);
  ASSERT_TRUE(patches.ok()) << patches.status();
  ASSERT_EQ(patches->size(), 3);  // 1 shared brotli and 2 iftb.

  for (PatchEncoding target_encoding :
       {PER_TABLE_SHARED_BROTLI_ENCODING, IFTB_ENCODING}) {
    for (const auto& p : *patches) {
      std::string url = p.first;
      PatchEncoding encoding = p.second;
      uint32_t id = std::stoul(url);

      if (encoding != target_encoding) {
        continue;
      }

      std::vector<FontData> patch_data;
      patch_data.resize(1);
      if (id <= 4) {
        patch_data[0].shallow_copy(iftb_patches_[id]);
      } else {
        auto it = encoder.Patches().find(id);
        ASSERT_TRUE(it != encoder.Patches().end());
        patch_data[0].shallow_copy(it->second);
      }

      auto s = client->ApplyPatches(patch_data, encoding);
      ASSERT_TRUE(s.ok()) << s;
    }
  }

  codepoints = ToCodepointsSet(client->GetFontData());
  ASSERT_TRUE(codepoints.contains(chunk0_cp));
  ASSERT_TRUE(codepoints.contains(chunk1_cp));
  ASSERT_FALSE(codepoints.contains(chunk2_cp));
  ASSERT_TRUE(codepoints.contains(chunk3_cp));
  ASSERT_TRUE(codepoints.contains(chunk4_cp));

  // TODO(garretrieger): check glyph presence as well.
  //   - We can extract the functions in iftb_binary_patch_test for dealing with
  //   glyf/loca.
}

}  // namespace ift