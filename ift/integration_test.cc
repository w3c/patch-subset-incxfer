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

using absl::flat_hash_set;
using absl::StrCat;
using ift::encoder::Encoder;
using ift::IFTClient;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using ift::proto::PatchEncoding;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::IFTB_ENCODING;

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

    hb_map_t* gid_to_unicode = GidToUnicodeMap(face);

    iftb_patches_.resize(5);
    iftb_subset_gids_.resize(5);
    iftb_subsets_.resize(5);
    for (int i = 1; i <= 4; i++) {
      std::string name =
          StrCat("ift/testdata/NotoSansJP-Regular.subset_iftb/chunk", i, ".br");
      blob = hb_blob_create_from_file(name.c_str());
      assert(hb_blob_get_length(blob) > 0);
      iftb_patches_[i].set(blob);
      hb_blob_destroy(blob);
    }

    iftb_subset_gids_[1] = {158, 160, 165, 166, 167, 168, 186};
    iftb_subset_gids_[2] = {159, 162, 171, 172, 175, 177, 184};
    iftb_subset_gids_[3] = {169};
    iftb_subset_gids_[4] = {161, 163, 164, 170, 173, 174, 176, 178,
                            179, 180, 181, 182, 183, 185, 187};

    for (int i = 1; i <= 4; i++) {
      for (uint32_t gid : iftb_subset_gids_[i]) {
        uint32_t cp = hb_map_get(gid_to_unicode, gid);
        assert(cp != (uint32_t) -1);
        iftb_subsets_[i].insert(cp);
      }
    }

    sbr_subsets_.resize(3);
    std::copy(iftb_subsets_[1].begin(), iftb_subsets_[1].end(),
              std::inserter(sbr_subsets_[0], sbr_subsets_[0].begin()));

    std::copy(iftb_subsets_[2].begin(), iftb_subsets_[2].end(),
              std::inserter(sbr_subsets_[1], sbr_subsets_[1].begin()));

    std::copy(iftb_subsets_[3].begin(), iftb_subsets_[3].end(),
              std::inserter(sbr_subsets_[2], sbr_subsets_[2].begin()));
    std::copy(iftb_subsets_[4].begin(), iftb_subsets_[4].end(),
              std::inserter(sbr_subsets_[2], sbr_subsets_[2].begin()));

    hb_map_destroy(gid_to_unicode);
  }

  hb_map_t* GidToUnicodeMap(hb_face_t* face) {
    hb_map_t* unicode_to_gid = hb_map_create ();
    hb_face_collect_nominal_glyph_mapping(face, unicode_to_gid, nullptr);

    hb_map_t* gid_to_unicode = hb_map_create();
    int index = -1;
    uint32_t cp = HB_MAP_VALUE_INVALID;
    uint32_t gid = HB_MAP_VALUE_INVALID;
    while (hb_map_next(unicode_to_gid, &index, &cp, &gid)) {
      hb_map_set(gid_to_unicode, gid, cp);
    }

    hb_map_destroy(unicode_to_gid);
    return gid_to_unicode;
  }

  FontData noto_sans_jp_;
  std::vector<FontData> iftb_patches_;
  std::vector<flat_hash_set<uint32_t>> iftb_subset_gids_;
  std::vector<flat_hash_set<uint32_t>> iftb_subsets_;
  std::vector<flat_hash_set<uint32_t>> sbr_subsets_;
};

TEST_F(IntegrationTest, MixedMode) {
  Encoder encoder;
  encoder.SetUrlTemplate("$2$1");
  auto sc = encoder.SetId({
    0x3c2bfda0,
    0x890625c9,
    0x40c644de,
    0xb1195627
  });
  ASSERT_TRUE(sc.ok()) << sc;

  for (int i = 1; i <= 4; i++) {
    encoder.AddExistingIftbPatch(i, iftb_subsets_[i]);
  }

  hb_face_t* face = noto_sans_jp_.reference_face();
  std::vector<const absl::flat_hash_set<hb_codepoint_t>*> subsets = {
      &(sbr_subsets_[1]), &(sbr_subsets_[2])};
  auto encoded = encoder.Encode(face, sbr_subsets_[0], subsets);
  hb_face_destroy(face);
  ASSERT_TRUE(encoded.ok()) << encoded.status();

  hb_set_unique_ptr unicodes = make_hb_set();
  face = encoded->reference_face();
  hb_face_collect_unicodes(face, unicodes.get());
  for (uint32_t cp : sbr_subsets_[0]) {
    ASSERT_TRUE(hb_set_has(unicodes.get(), cp));
  }
  for (uint32_t cp : sbr_subsets_[1]) {
    ASSERT_FALSE(hb_set_has(unicodes.get(), cp));
  }
  hb_face_destroy(face);

  auto client = IFTClient::NewClient(std::move(*encoded));
  ASSERT_TRUE(client.ok()) << client.status();

  // gids 161, 163, 164
  hb_set_unique_ptr input = make_hb_set(3, 0xe3, 0xe5, 0xe6);
  auto patches = client->PatchUrlsFor(*input);
  ASSERT_TRUE(patches.ok()) << patches.status();

  for (PatchEncoding target_encoding : {PER_TABLE_SHARED_BROTLI_ENCODING, IFTB_ENCODING}) {
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

      printf("Applying patch %u with encoding %u\n", id, encoding);
      auto s = client->ApplyPatches(patch_data, encoding);
      ASSERT_TRUE(s.ok()) << s;
    }
  }

  face = client->GetFontData().reference_face();
  hb_set_clear(unicodes.get());
  hb_face_collect_unicodes(face, unicodes.get());
  hb_face_destroy(face);

  ASSERT_FALSE(hb_set_has(unicodes.get(), 159));
  ASSERT_TRUE(hb_set_has(unicodes.get(), 160));
  ASSERT_TRUE(hb_set_has(unicodes.get(), 161));
  ASSERT_TRUE(hb_set_has(unicodes.get(), 163));
  ASSERT_TRUE(hb_set_has(unicodes.get(), 164));
  ASSERT_TRUE(hb_set_has(unicodes.get(), 169));
  
  // TODO check glyph presence as well.
  //   - We can extract the functions in iftb_binary_patch_test for dealing with glyf/loca.
  //   - Need to ensure loca is long to use those.
}

}  // namespace ift