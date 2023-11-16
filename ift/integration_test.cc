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
using absl::Status;
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

    for (int i = 1; i <= 4; i++) {
      auto sc = encoder.AddExistingIftbPatch(i, iftb_patches_[i]);
      if (!sc.ok()) {
        return sc;
      }
    }

    return absl::OkStatus();
  }

  Status AddPatches(IFTClient& client, Encoder& encoder) {
    auto patches = client.PatchesNeeded();
    for (const auto& id : patches) {
      FontData patch_data;
      if (id <= 4) {
        patch_data.shallow_copy(iftb_patches_[id]);
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

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;

  uint32_t chunk0_cp = 0x47;
  uint32_t chunk1_cp = 0xb7;
  uint32_t chunk2_cp = 0xb2;
  uint32_t chunk3_cp = 0xeb;
  uint32_t chunk4_cp = 0xa8;
};

// TODO(garretrieger): add IFTB only test case.
// TODO(garretrieger): add shared brotli only test case.
// TODO(garretrieger): add test case where changing the
//  target codepoints causes the dependent patch selection
//  to change.

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

  sc = AddPatches(*client, encoder);
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

  // TODO(garretrieger): check glyph presence as well.
  //   - We can extract the functions in iftb_binary_patch_test for dealing with
  //   glyf/loca.
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

  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::READY);

  // Phase 2
  sc = client->AddDesiredCodepoints({chunk3_cp});
  ASSERT_TRUE(sc.ok()) << sc;

  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 2);  // 1 shared brotli and 1 iftb.

  sc = AddPatches(*client, encoder);
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
  // TODO(garretrieger): also check glyph presence
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

  sc = AddPatches(*client, encoder);
  ASSERT_TRUE(sc.ok()) << sc;

  auto state = client->Process();
  ASSERT_TRUE(state.ok()) << state.status();
  ASSERT_EQ(*state, IFTClient::NEEDS_PATCHES);

  // the first application round would have added one of {3}
  // and {4}. now that one is applied, the second is still needed.
  patches = client->PatchesNeeded();
  ASSERT_EQ(patches.size(), 1);  // 1 shared brotli

  sc = AddPatches(*client, encoder);
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