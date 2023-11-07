#include "ift/per_table_brotli_binary_diff.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"

using absl::StatusOr;
using absl::string_view;
using common::FontHelper;
using ift::proto::PerTablePatch;
using patch_subset::BrotliBinaryPatch;
using patch_subset::FontData;

namespace ift {

class PerTableBrotliBinaryDiffTest : public ::testing::Test {
 protected:
  PerTableBrotliBinaryDiffTest() {
    // TODO
  }

  hb_tag_t tag1 = HB_TAG('t', 'a', 'g', '1');
  hb_tag_t tag2 = HB_TAG('t', 'a', 'g', '2');
  hb_tag_t tag3 = HB_TAG('t', 'a', 'g', '3');

  std::string tag1_str = FontHelper::ToString(tag1);
  std::string tag2_str = FontHelper::ToString(tag2);
  std::string tag3_str = FontHelper::ToString(tag3);
};

StatusOr<std::string> patch_table(std::string before, std::string table_patch) {
  FontData base, patch;
  base.copy(before.data(), before.size());
  patch.copy(table_patch.data(), table_patch.size());

  FontData derived;
  BrotliBinaryPatch patcher;
  auto sc = patcher.Patch(base, patch, &derived);
  if (!sc.ok()) {
    return sc;
  }

  return derived.string();
}

TEST_F(PerTableBrotliBinaryDiffTest, BasicDiff) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
      {tag2, "baar"},
  });

  PerTableBrotliBinaryDiff differ;
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_TRUE(patch_proto.removed_tables().empty());

  ASSERT_EQ(patch_proto.table_patches_size(), 2);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "fooo");

  new_table = patch_table("bar", patch_proto.table_patches().at(tag2_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "baar");
}

TEST_F(PerTableBrotliBinaryDiffTest, RemoveTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "foo"},
  });

  PerTableBrotliBinaryDiff differ;
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_EQ(patch_proto.removed_tables().size(), 1);
  ASSERT_EQ(patch_proto.table_patches_size(), 1);

  ASSERT_EQ(patch_proto.removed_tables().at(0), tag2_str);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "foo");
}

TEST_F(PerTableBrotliBinaryDiffTest, AddTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  PerTableBrotliBinaryDiff differ;
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_TRUE(patch_proto.removed_tables().empty());

  ASSERT_EQ(patch_proto.table_patches_size(), 2);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "foo");

  new_table = patch_table("", patch_proto.table_patches().at(tag2_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "bar");
}

TEST_F(PerTableBrotliBinaryDiffTest, FilteredDiff) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
      {tag3, "baz"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
      {tag2, "baar"},
      {tag3, "baaz"},
  });

  PerTableBrotliBinaryDiff differ({tag2_str.c_str()});
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_TRUE(patch_proto.removed_tables().empty());

  ASSERT_EQ(patch_proto.table_patches_size(), 2);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "fooo");

  new_table = patch_table("baz", patch_proto.table_patches().at(tag3_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "baaz");
}

TEST_F(PerTableBrotliBinaryDiffTest, FilteredDiff_WithRemove) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
      {tag3, "baz"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
  });

  PerTableBrotliBinaryDiff differ({tag2_str.c_str()});
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_EQ(patch_proto.removed_tables().size(), 1);
  ASSERT_EQ(patch_proto.table_patches_size(), 1);

  ASSERT_EQ(patch_proto.removed_tables().at(0), tag3_str);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "fooo");
}

}  // namespace ift
