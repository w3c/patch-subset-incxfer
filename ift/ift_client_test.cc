#include "ift/ift_client.h"

#include "absl/container/btree_set.h"
#include "gtest/gtest.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using absl::btree_set;
using absl::IsInvalidArgument;
using ift::proto::IFT;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BrotliBinaryDiff;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using patch_subset::SparseBitSet;

namespace ift {

class IFTClientTest : public ::testing::Test {
 protected:
  IFTClientTest() {
    IFT sample;
    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(9);
    m->set_patch_encoding(IFTB_ENCODING);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(42);
    m->set_patch_encoding(SHARED_BROTLI_ENCODING);

    sample.set_url_template("https://localhost/patches/$2$1.patch");

    hb_blob_t* blob =
        hb_blob_create_from_file("patch_subset/testdata/Roboto-Regular.ab.ttf");
    hb_face_t* face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);

    auto font = IFTTable::AddToFont(face, sample);
    sample_font.set(font->reference_face());
  }

  IFTClient client;
  FontData sample_font;
};

TEST_F(IFTClientTest, PatchUrls) {
  hb_set_unique_ptr codepoints_1 = make_hb_set(1, 30);
  hb_set_unique_ptr codepoints_2 = make_hb_set(2, 55, 57);
  hb_set_unique_ptr codepoints_3 = make_hb_set(2, 32, 56);
  hb_set_unique_ptr codepoints_4 = make_hb_set(0);
  hb_set_unique_ptr codepoints_5 = make_hb_set(1, 112);
  hb_set_unique_ptr codepoints_6 = make_hb_set(1, 30, 112);

  std::string url_1 = "https://localhost/patches/09.patch";
  std::string url_2 = "https://localhost/patches/2a.patch";

  patch_set expected_1{std::pair(url_1, IFTB_ENCODING)};
  patch_set expected_2{std::pair(url_2, SHARED_BROTLI_ENCODING)};
  patch_set expected_3{std::pair(url_1, IFTB_ENCODING),
                       std::pair(url_2, SHARED_BROTLI_ENCODING)};
  patch_set expected_4{};
  patch_set expected_5{};
  patch_set expected_6{std::pair(url_1, IFTB_ENCODING)};

  auto r = client.PatchUrlsFor(sample_font, *codepoints_1);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_1, *r);

  r = client.PatchUrlsFor(sample_font, *codepoints_2);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_2, *r);

  r = client.PatchUrlsFor(sample_font, *codepoints_3);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_3, *r);

  r = client.PatchUrlsFor(sample_font, *codepoints_4);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_4, *r);

  r = client.PatchUrlsFor(sample_font, *codepoints_5);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_5, *r);

  r = client.PatchUrlsFor(sample_font, *codepoints_6);
  ASSERT_TRUE(r.ok()) << r.status();
  ASSERT_EQ(expected_6, *r);
}

TEST_F(IFTClientTest, ApplyPatches_IFTB) {
  FontData base;
  FontData patch;
  std::vector<FontData> patches;
  patches.push_back(std::move(patch));

  IFTClient client;
  auto s = client.ApplyPatches(base, patches, IFTB_ENCODING);
  ASSERT_TRUE(absl::IsUnimplemented(s.status())) << s.status();
}

TEST_F(IFTClientTest, ApplyPatches_SharedBrotli) {
  std::string d1 = "abc";
  std::string d2 = "abcdef";
  std::string d3 = "abcdefhij";
  FontData f1(d1);
  FontData f2(d2);
  FontData f3(d3);

  BrotliBinaryDiff differ;
  FontData f1_to_f2;
  auto s = differ.Diff(f1, f2, &f1_to_f2);
  ASSERT_TRUE(s.ok()) << s;

  FontData f2_to_f3;
  s = differ.Diff(f2, f3, &f2_to_f3);
  ASSERT_TRUE(s.ok()) << s;

  IFTClient client;

  {
    std::vector<FontData> patch_set_1;
    patch_set_1.emplace_back(f1_to_f2.str());
    auto s = client.ApplyPatches(f1, patch_set_1, SHARED_BROTLI_ENCODING);
    ASSERT_TRUE(s.ok()) << s.status();
    ASSERT_EQ(s->str(), f2.str());

    std::vector<FontData> patch_set_2;
    patch_set_2.emplace_back(f2_to_f3.str());
    s = client.ApplyPatches(f2, patch_set_2, SHARED_BROTLI_ENCODING);
    ASSERT_TRUE(s.ok()) << s.status();
    ASSERT_EQ(s->str(), f3.str());
  }

  {
    std::vector<FontData> patch_set_1;
    patch_set_1.emplace_back(f1_to_f2.str());
    patch_set_1.emplace_back(f2_to_f3.str());
    auto s = client.ApplyPatches(f1, patch_set_1, SHARED_BROTLI_ENCODING);
    ASSERT_TRUE(IsInvalidArgument(s.status())) << s.status();
  }
}

}  // namespace ift