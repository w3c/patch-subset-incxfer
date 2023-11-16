#include "patch_subset/compressed_set.h"

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/cbor/compressed_set.h"

namespace patch_subset {

using absl::Span;
using absl::Status;
using common::BF8;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::make_hb_set_from_ranges;
using common::SparseBitSet;
using testing::Eq;
using testing::Pointwise;

class CompressedSetTest : public ::testing::Test {
 protected:
  CompressedSetTest() {}

  ~CompressedSetTest() override {}

  void SetUp() override {}

  void Encode(hb_set_unique_ptr input) {
    encoded_ = std::make_unique<patch_subset::cbor::CompressedSet>();
    CompressedSet::Encode(*input, *encoded_);

    hb_set_unique_ptr decoded = make_hb_set();
    EXPECT_EQ(CompressedSet::Decode(*encoded_, decoded.get()),
              absl::OkStatus());
    EXPECT_TRUE(hb_set_is_equal(decoded.get(), input.get()));
  }

  void CheckSparseSet(hb_set_unique_ptr set) {
    std::string expected = SparseBitSet::Encode(*set, BF8);
    EXPECT_THAT(encoded_->SparseBitSetBytes(), Pointwise(Eq(), expected));
  }

  void CheckDeltaList(const patch_subset::cbor::range_vector &deltas) {
    EXPECT_EQ(encoded_->Ranges(), deltas);
  }

  std::unique_ptr<patch_subset::cbor::CompressedSet> encoded_;
};

// TODO(garretrieger): null test
// TODO(garretrieger): invalid sparse bit set and invalid ranges test.

TEST_F(CompressedSetTest, EncodeEmpty) {
  Encode(make_hb_set(0));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(patch_subset::cbor::range_vector());
}

TEST_F(CompressedSetTest, EncodeAllSparse) {
  Encode(make_hb_set(3, 1, 5, 13));
  CheckSparseSet(make_hb_set(3, 1, 5, 13));
  CheckDeltaList(patch_subset::cbor::range_vector());

  Encode(make_hb_set(1, 40000));
  CheckSparseSet(make_hb_set(1, 40000));
  CheckDeltaList(patch_subset::cbor::range_vector());

  Encode(make_hb_set(2, 128, 143));
  CheckSparseSet(make_hb_set(2, 128, 143));
  CheckDeltaList(patch_subset::cbor::range_vector());
}

TEST_F(CompressedSetTest, EncodeAllRanges) {
  Encode(make_hb_set_from_ranges(1, 5, 50));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(patch_subset::cbor::range_vector{{5, 50}});

  Encode(make_hb_set_from_ranges(2, 5, 50, 53, 100));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(patch_subset::cbor::range_vector{{5, 50}, {53, 100}});
}

TEST_F(CompressedSetTest, EncodeSparseVsRange) {
  // 2 + 1 bytes as a range vs 3 bytes as a sparse -> Range
  Encode(make_hb_set_from_ranges(1, 1000, 1023));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(patch_subset::cbor::range_vector{{1000, 1023}});

  // 2 + 1 bytes as a range vs 2 bytes as a sparse -> Sparse
  Encode(make_hb_set_from_ranges(1, 1000, 1015));
  CheckSparseSet(make_hb_set_from_ranges(1, 1000, 1015));
  CheckDeltaList(patch_subset::cbor::range_vector());
}

TEST_F(CompressedSetTest, EncodeMixed) {
  hb_set_unique_ptr set = make_hb_set_from_ranges(1, 1000, 1023);
  hb_set_add(set.get(), 990);
  hb_set_add(set.get(), 1025);
  Encode(std::move(set));
  CheckSparseSet(make_hb_set(2, 990, 1025));
  CheckDeltaList(patch_subset::cbor::range_vector{{1000, 1023}});

  set = make_hb_set();
  hb_set_add_range(set.get(), 1, 100);  // 100
  hb_set_add(set.get(), 113);
  hb_set_add(set.get(), 115);
  hb_set_add_range(set.get(), 201, 250);  // 50
  hb_set_add_range(set.get(), 252, 301);  // 50
  hb_set_add(set.get(), 315);
  hb_set_add_range(set.get(), 401, 500);  // 100
  Encode(std::move(set));
  CheckSparseSet(make_hb_set(3, 113, 115, 315));
  CheckDeltaList(patch_subset::cbor::range_vector{
      {1, 100}, {201, 250}, {252, 301}, {401, 500}});

  set = make_hb_set();
  hb_set_add(set.get(), 113);
  hb_set_add(set.get(), 115);
  hb_set_add_range(set.get(), 201, 250);
  hb_set_add_range(set.get(), 252, 301);
  hb_set_add(set.get(), 315);

  Encode(std::move(set));
  CheckSparseSet(make_hb_set(3, 113, 115, 315));
  CheckDeltaList(patch_subset::cbor::range_vector{{201, 250}, {252, 301}});
}

TEST_F(CompressedSetTest, EncodeAdjacentRanges) {
  // range: 2 bytes + 1 bytes <= hybrid: 3 bytes
  Encode(make_hb_set_from_ranges(1, 132, 155));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(patch_subset::cbor::range_vector{{132, 155}});

  // range: 2 bytes + 1 bytes > hybrid: 2 bytes
  hb_set_unique_ptr set = make_hb_set_from_ranges(1, 128, 129);
  hb_set_add_range(set.get(), 132, 155);
  Encode(std::move(set));

  set = make_hb_set_from_ranges(1, 128, 129);
  hb_set_add_range(set.get(), 132, 155);
  CheckSparseSet(std::move(set));
  CheckDeltaList(patch_subset::cbor::range_vector());

  // range: 2 bytes + 1 bytes > hybrid: 2 bytes
  set = make_hb_set_from_ranges(1, 132, 155);
  hb_set_add_range(set.get(), 157, 160);
  Encode(std::move(set));

  set = make_hb_set_from_ranges(1, 132, 155);
  hb_set_add_range(set.get(), 157, 160);
  CheckSparseSet(std::move(set));
  CheckDeltaList(patch_subset::cbor::range_vector());
}

}  // namespace patch_subset
