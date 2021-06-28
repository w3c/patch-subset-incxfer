#include "patch_subset/compressed_set.h"

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "common/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using ::absl::Span;

using ::testing::Eq;
using ::testing::Pointwise;

namespace patch_subset {

class CompressedSetTest : public ::testing::Test {
 protected:
  CompressedSetTest() {}

  ~CompressedSetTest() override {}

  void SetUp() override {}

  void Encode(hb_set_unique_ptr input) {
    encoded_ = std::make_unique<CompressedSetProto>();
    CompressedSet::Encode(*input, encoded_.get());

    hb_set_unique_ptr decoded = make_hb_set();
    EXPECT_EQ(CompressedSet::Decode(*encoded_, decoded.get()), StatusCode::kOk);
    EXPECT_TRUE(hb_set_is_equal(decoded.get(), input.get()));
  }

  void CheckSparseSet(hb_set_unique_ptr set) {
    std::string expected = SparseBitSet::Encode(*set);
    EXPECT_THAT(encoded_->sparse_bit_set(), Pointwise(Eq(), expected));
  }

  void CheckDeltaList(Span<const int> deltas) {
    EXPECT_THAT(encoded_->range_deltas(), Pointwise(Eq(), deltas));
  }

  std::unique_ptr<CompressedSetProto> encoded_;
};

// TODO(garretrieger): null test
// TODO(garretrieger): invalid sparse bit set and invalid ranges test.

TEST_F(CompressedSetTest, EncodeEmpty) {
  Encode(make_hb_set(0));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(std::vector<int>());
}

TEST_F(CompressedSetTest, EncodeAllSparse) {
  Encode(make_hb_set(3, 1, 5, 13));
  CheckSparseSet(make_hb_set(3, 1, 5, 13));
  CheckDeltaList(std::vector<int>());

  Encode(make_hb_set(1, 40000));
  CheckSparseSet(make_hb_set(1, 40000));
  CheckDeltaList(std::vector<int>());

  Encode(make_hb_set(2, 128, 143));
  CheckSparseSet(make_hb_set(2, 128, 143));
  CheckDeltaList(std::vector<int>());
}

TEST_F(CompressedSetTest, EncodeAllRanges) {
  Encode(make_hb_set_from_ranges(1, 5, 50));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(std::vector<int>{5, 45});

  Encode(make_hb_set_from_ranges(2, 5, 50, 53, 100));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(std::vector<int>{5, 45, 3, 47});
}

TEST_F(CompressedSetTest, EncodeSparseVsRange) {
  // 2 + 1 bytes as a range vs 3 bytes as a sparse -> Range
  Encode(make_hb_set_from_ranges(1, 1000, 1023));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(std::vector<int>{1000, 23});

  // 2 + 1 bytes as a range vs 2 bytes as a sparse -> Sparse
  Encode(make_hb_set_from_ranges(1, 1000, 1015));
  CheckSparseSet(make_hb_set_from_ranges(1, 1000, 1015));
  CheckDeltaList(std::vector<int>());
}

TEST_F(CompressedSetTest, EncodeMixed) {
  hb_set_unique_ptr set = make_hb_set_from_ranges(1, 1000, 1023);
  hb_set_add(set.get(), 990);
  hb_set_add(set.get(), 1025);
  Encode(std::move(set));
  CheckSparseSet(make_hb_set(2, 990, 1025));
  CheckDeltaList(std::vector<int>{1000, 23});

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
  CheckDeltaList(std::vector<int>{
      1, 99,    // 1 - 100
      101, 49,  // 201 - 250
      2, 49,    // 252 - 301
      100, 99   // 401 - 500
  });

  set = make_hb_set();
  hb_set_add(set.get(), 113);
  hb_set_add(set.get(), 115);
  hb_set_add_range(set.get(), 201, 250);
  hb_set_add_range(set.get(), 252, 301);
  hb_set_add(set.get(), 315);

  Encode(std::move(set));
  CheckSparseSet(make_hb_set(3, 113, 115, 315));
  CheckDeltaList(std::vector<int>{
      201, 49,  // 201 - 250
      2, 49     // 252 - 301
  });
}

TEST_F(CompressedSetTest, EncodeAdjacentRanges) {
  // range: 2 bytes + 1 bytes <= hybrid: 3 bytes
  Encode(make_hb_set_from_ranges(1, 132, 155));
  CheckSparseSet(make_hb_set(0));
  CheckDeltaList(std::vector<int>{132, 23});

  // range: 2 bytes + 1 bytes > hybrid: 2 bytes
  hb_set_unique_ptr set = make_hb_set_from_ranges(1, 128, 129);
  hb_set_add_range(set.get(), 132, 155);
  Encode(std::move(set));

  set = make_hb_set_from_ranges(1, 128, 129);
  hb_set_add_range(set.get(), 132, 155);
  CheckSparseSet(std::move(set));
  CheckDeltaList(std::vector<int>());

  // range: 2 bytes + 1 bytes > hybrid: 2 bytes
  set = make_hb_set_from_ranges(1, 132, 155);
  hb_set_add_range(set.get(), 157, 160);
  Encode(std::move(set));

  set = make_hb_set_from_ranges(1, 132, 155);
  hb_set_add_range(set.get(), 157, 160);
  CheckSparseSet(std::move(set));
  CheckDeltaList(std::vector<int>());
}

}  // namespace patch_subset
