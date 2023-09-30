#include "patch_subset/proto/ift_table.h"

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using patch_subset::SparseBitSet;

namespace patch_subset::proto {

class IFTTableTest : public ::testing::Test {
 protected:
  IFTTableTest() {
    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(1);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id(2);
  }

  IFT sample;
};

TEST_F(IFTTableTest, Mapping) {
  auto table = IFTTable::FromProto(sample);
  ASSERT_TRUE(table.ok()) << table.status();

  flat_hash_map<uint32_t, uint32_t> expected = {
      {30, 1}, {32, 1}, {55, 2}, {56, 2}, {57, 2},
  };

  ASSERT_EQ(table->get_patch_map(), expected);
}

}  // namespace patch_subset::proto
