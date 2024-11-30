#include "ift/table_keyed_diff.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/brotli_binary_diff.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb.h"

using absl::btree_set;
using absl::StatusOr;
using absl::string_view;
using common::BrotliBinaryDiff;
using common::FontData;
using common::FontHelper;

namespace ift {

class TableKeyedDiffTest : public ::testing::Test {
 protected:
  TableKeyedDiffTest() {
    // TODO
  }

  hb_tag_t tag1 = HB_TAG('t', 'a', 'g', '1');
  hb_tag_t tag2 = HB_TAG('t', 'a', 'g', '2');
  hb_tag_t tag3 = HB_TAG('t', 'a', 'g', '3');

  std::string tag1_str = FontHelper::ToString(tag1);
  std::string tag2_str = FontHelper::ToString(tag2);
  std::string tag3_str = FontHelper::ToString(tag3);
};


StatusOr<std::string> diff_table(std::string before, std::string after) {
  FontData base, derived;
  base.copy(before);
  derived.copy(after);
  

  ;
  BrotliBinaryDiff differ;
  FontData patch;
  auto sc = differ.Diff(base, derived, &patch);
  if (!sc.ok()) {
    return sc;
  }

  return patch.string();
}

TEST_F(TableKeyedDiffTest, BasicDiff) {
  char second_offset = 26 + 9 + diff_table("foo", "fooo")->length();
  char third_offset = second_offset + 9 + diff_table("bar", "baar")->length();
  std::string expected = std::string {
    'i', 'f', 't', 'k',
    0, 0, 0, 0, // reserved

    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0, // compat id

    0, 2, // patches count
    0, 0, 0, 26, // patches offset[0]
    0, 0, 0, second_offset, // patches offset[1]
    0, 0, 0, third_offset, // patches offset[2]

    't', 'a', 'g', '1',
    0, // flags
    0, 0, 0, 4, // uncompressed size
  } +
  *diff_table("foo", "fooo") +
  std::string {
    't', 'a', 'g', '2',
    0, // flags
    0, 0, 0, 4, // uncompressed size
  } +
  *diff_table("bar", "baar");

  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
      {tag2, "baar"},
  });

  TableKeyedDiff differ;
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;
  ASSERT_EQ(patch.string(), expected);
} 

/*
TODO reimplement these against the new format.
TEST_F(TableKeyedDiffTest, ReplacementDiff) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
      {tag2, "baar"},
  });

  TableKeyedDiff differ({}, {"tag2"});
  FontData patch;
  auto sc = differ.Diff(before, after, &patch);
  ASSERT_TRUE(sc.ok()) << sc;

  PerTablePatch patch_proto;
  ASSERT_TRUE(patch_proto.ParseFromArray(patch.data(), patch.size()));
  ASSERT_TRUE(patch_proto.removed_tables().empty());
  ASSERT_EQ(patch_proto.replaced_tables_size(), 1);
  ASSERT_EQ(patch_proto.replaced_tables().at(0), "tag2");

  ASSERT_EQ(patch_proto.table_patches_size(), 2);

  auto new_table = patch_table("foo", patch_proto.table_patches().at(tag1_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "fooo");

  new_table = patch_table("", patch_proto.table_patches().at(tag2_str));
  ASSERT_TRUE(new_table.ok()) << new_table.status();
  ASSERT_EQ(*new_table, "baar");
}

TEST_F(TableKeyedDiffTest, RemoveTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "foo"},
  });

  TableKeyedDiff differ;
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

TEST_F(TableKeyedDiffTest, AddTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
  });

  TableKeyedDiff differ;
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

TEST_F(TableKeyedDiffTest, FilteredDiff) {
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

  TableKeyedDiff differ({tag2_str.c_str()});
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

TEST_F(TableKeyedDiffTest, FilteredDiff_WithRemove) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "bar"},
      {tag3, "baz"},
  });

  FontData after = FontHelper::BuildFont({
      {tag1, "fooo"},
  });

  TableKeyedDiff differ({tag2_str.c_str()});
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
*/

}  // namespace ift
