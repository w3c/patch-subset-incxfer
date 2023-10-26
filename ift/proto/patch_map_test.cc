#include "ift/proto/patch_map.h"

#include <cstdio>
#include <cstring>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "common/font_helper.h"
#include "gtest/gtest.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/font_data.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using common::FontHelper;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using patch_subset::SparseBitSet;

namespace ift::proto {

class PatchMapTest : public ::testing::Test {
 protected:
  PatchMapTest() {
    sample.set_url_template("fonts/go/here");
    sample.set_default_patch_encoding(SHARED_BROTLI_ENCODING);

    auto m = sample.add_subset_mapping();
    hb_set_unique_ptr set = make_hb_set(2, 7, 9);
    m->set_bias(23);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);

    m = sample.add_subset_mapping();
    set = make_hb_set(3, 10, 11, 12);
    m->set_bias(45);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);
    m->set_patch_encoding(IFTB_ENCODING);

    overlap_sample = sample;

    m = overlap_sample.add_subset_mapping();
    set = make_hb_set(1, 55);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(0);

    complex_ids.set_url_template("fonts/go/here");
    complex_ids.set_default_patch_encoding(SHARED_BROTLI_ENCODING);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 0);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(-1);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 5);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(4);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 2);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(-4);

    m = complex_ids.add_subset_mapping();
    set = make_hb_set(1, 4);
    m->set_bias(0);
    m->set_codepoint_set(SparseBitSet::Encode(*set.get()));
    m->set_id_delta(1);
  }

  IFT empty;
  IFT sample;
  IFT overlap_sample;
  IFT complex_ids;
};

TEST_F(PatchMapTest, Empty) {
  auto map = PatchMap::FromProto(empty);
  ASSERT_TRUE(map.ok()) << map.status();
  PatchMap expected = {};
  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, GetEntries) {
  auto map = PatchMap::FromProto(sample);
  ASSERT_TRUE(map.ok()) << map.status();

  PatchMap::Entry entries[] = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
  };
  Span<const PatchMap::Entry> expected(entries);

  ASSERT_EQ(map->GetEntries(), expected);
}

TEST_F(PatchMapTest, Mapping) {
  auto map = PatchMap::FromProto(sample);
  ASSERT_TRUE(map.ok()) << map.status();

  PatchMap expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
  };

  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, Mapping_ComplexIds) {
  auto map = PatchMap::FromProto(complex_ids);
  ASSERT_TRUE(map.ok()) << map.status();

  PatchMap expected = {
      {{0}, 0, SHARED_BROTLI_ENCODING},
      {{5}, 5, SHARED_BROTLI_ENCODING},
      {{2}, 2, SHARED_BROTLI_ENCODING},
      {{4}, 4, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, Mapping_Overlaping) {
  auto map = PatchMap::FromProto(overlap_sample);
  ASSERT_TRUE(map.ok()) << map.status();

  PatchMap expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
      {{55}, 3, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, AddPatch) {
  auto map = PatchMap::FromProto(sample);
  ASSERT_TRUE(map.ok()) << map.status();

  map->AddEntry({77, 79, 80}, 5, SHARED_BROTLI_ENCODING);

  PatchMap expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
      {{77, 79, 80}, 5, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(*map, expected);

  map->AddEntry({1, 2, 3}, 3, IFTB_ENCODING);

  expected = {
      {{30, 32}, 1, SHARED_BROTLI_ENCODING},
      {{55, 56, 57}, 2, IFTB_ENCODING},
      {{77, 79, 80}, 5, SHARED_BROTLI_ENCODING},
      {{1, 2, 3}, 3, IFTB_ENCODING},
  };

  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, RemoveEntries) {
  auto map = PatchMap::FromProto(sample);
  ASSERT_TRUE(map.ok()) << map.status();

  map->RemoveEntries(1);

  PatchMap expected = {
      {{55, 56, 57}, 2, IFTB_ENCODING},
  };

  ASSERT_EQ(*map, expected);
}

TEST_F(PatchMapTest, RemoveEntries_Multiple) {
  PatchMap map;
  map.AddEntry({1, 2}, 3, SHARED_BROTLI_ENCODING);
  map.AddEntry({3, 4}, 1, SHARED_BROTLI_ENCODING);
  map.AddEntry({5, 6}, 2, SHARED_BROTLI_ENCODING);
  map.AddEntry({7, 8}, 3, SHARED_BROTLI_ENCODING);
  map.AddEntry({9, 10}, 5, SHARED_BROTLI_ENCODING);

  map.RemoveEntries(3);

  PatchMap expected = {
      {{3, 4}, 1, SHARED_BROTLI_ENCODING},
      {{5, 6}, 2, SHARED_BROTLI_ENCODING},
      {{9, 10}, 5, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(map, expected);
}

TEST_F(PatchMapTest, RemoveEntries_NotFound) {
  PatchMap map;
  map.AddEntry({1, 2}, 3, SHARED_BROTLI_ENCODING);
  map.AddEntry({3, 4}, 1, SHARED_BROTLI_ENCODING);
  map.AddEntry({5, 6}, 2, SHARED_BROTLI_ENCODING);
  map.AddEntry({7, 8}, 3, SHARED_BROTLI_ENCODING);
  map.AddEntry({9, 10}, 5, SHARED_BROTLI_ENCODING);

  map.RemoveEntries(7);

  PatchMap expected = {
      {{1, 2}, 3, SHARED_BROTLI_ENCODING},  {{3, 4}, 1, SHARED_BROTLI_ENCODING},
      {{5, 6}, 2, SHARED_BROTLI_ENCODING},  {{7, 8}, 3, SHARED_BROTLI_ENCODING},
      {{9, 10}, 5, SHARED_BROTLI_ENCODING},
  };

  ASSERT_EQ(map, expected);
}

TEST_F(PatchMapTest, RemovePatches_All) {
  auto map = PatchMap::FromProto(sample);
  ASSERT_TRUE(map.ok()) << map.status();

  map->RemoveEntries(1);
  map->RemoveEntries(2);

  PatchMap expected = {};
  ASSERT_EQ(*map, expected);
}

}  // namespace ift::proto
