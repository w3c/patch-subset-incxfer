#include "ift/per_table_brotli_binary_patch.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/brotli_binary_diff.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"

using absl::StatusOr;
using absl::string_view;
using common::BrotliBinaryDiff;
using common::BrotliBinaryPatch;
using common::FontData;
using common::FontHelper;
using ift::proto::PerTablePatch;

namespace ift {

class PerTableBrotliBinaryPatchTest : public ::testing::Test {
 protected:
  PerTableBrotliBinaryPatchTest() {
    BrotliBinaryDiff differ;

    FontData foo, bar, abc, def, hello, empty;
    foo.copy("foo");
    bar.copy("bar");
    abc.copy("abc");
    def.copy("def");
    hello.copy("hello");

    auto sc = differ.Diff(foo, bar, &foo_to_bar);
    sc.Update(differ.Diff(abc, def, &abc_to_def));
    sc.Update(differ.Diff(empty, def, &empty_to_def));
    sc.Update(differ.Diff(empty, hello, &empty_to_hello));

    assert(sc.ok());
  }

  hb_tag_t tag1 = HB_TAG('t', 'a', 'g', '1');
  hb_tag_t tag2 = HB_TAG('t', 'a', 'g', '2');
  hb_tag_t tag3 = HB_TAG('t', 'a', 'g', '3');

  std::string tag1_str = FontHelper::ToString(tag1);
  std::string tag2_str = FontHelper::ToString(tag2);
  std::string tag3_str = FontHelper::ToString(tag3);

  FontData foo_to_bar;
  FontData abc_to_def;
  FontData empty_to_def;
  FontData empty_to_hello;

  PerTableBrotliBinaryPatch patcher;
};

TEST_F(PerTableBrotliBinaryPatchTest, BasicPatch) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
      {tag2, "def"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  (*patch_proto.mutable_table_patches())[tag2_str] = abc_to_def.string();
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, ReplaceTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
      {tag2, "hello"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  (*patch_proto.mutable_table_patches())[tag2_str] = empty_to_hello.string();
  patch_proto.add_replaced_tables(tag2_str);

  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, AddTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
      {tag2, "def"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  (*patch_proto.mutable_table_patches())[tag2_str] = empty_to_def.string();
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, PassThroughTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
      {tag2, "abc"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, RemoveTable) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  patch_proto.add_removed_tables(tag2_str);
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, RemoveTable_TakesPriorityOverPatch) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "bar"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag1_str] = foo_to_bar.string();
  (*patch_proto.mutable_table_patches())[tag2_str] = abc_to_def.string();
  patch_proto.add_removed_tables(tag2_str);
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

TEST_F(PerTableBrotliBinaryPatchTest, MixedOperations) {
  FontData before = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag2, "def"},
      {tag3, "abc"},
  });
  FontData after = FontHelper::BuildFont({
      {tag1, "foo"},
      {tag3, "def"},
  });

  PerTablePatch patch_proto;
  (*patch_proto.mutable_table_patches())[tag3_str] = abc_to_def.string();
  patch_proto.add_removed_tables(tag2_str);
  std::string patch = patch_proto.SerializeAsString();
  FontData patch_data;
  patch_data.copy(patch.data(), patch.size());

  FontData result;
  auto sc = patcher.Patch(before, patch_data, &result);
  ASSERT_TRUE(sc.ok()) << sc;

  ASSERT_EQ(after.str(), result.str());
}

}  // namespace ift
