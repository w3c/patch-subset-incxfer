#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "util/brotli_stream.h"
#include "patch_subset/brotli_binary_patch.h"

using ::absl::Span;
using ::util::BrotliStream;
using ::patch_subset::BrotliBinaryPatch;

namespace patch_subset {

class BrotliStreamTest : public ::testing::Test {
 protected:
  BrotliStreamTest() {}

  ~BrotliStreamTest() override {}

  void SetUp() override {
  }
};

TEST_F(BrotliStreamTest, InsertUncompressed) {
  BrotliStream stream(22);
  char data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
  stream.insert_uncompressed(Span<const uint8_t>((const uint8_t*) data, 11));
  stream.end_stream();

  BrotliBinaryPatch patcher;
  FontData empty;
  FontData patch;
  patch.copy((const char*) stream.compressed_data().data(), stream.compressed_data().size());

  // TODO: move to helper.
  FontData uncompressed;
  EXPECT_EQ(patcher.Patch(empty, patch, &uncompressed), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(uncompressed),
            Span<const char>(data));
}

}  // namespace patch_subset
