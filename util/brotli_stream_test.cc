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

void CheckDecompressesTo(const BrotliStream& stream,
                         Span<const uint8_t> expected,
                         Span<const uint8_t> dict_data=Span<const uint8_t>())
{
  BrotliBinaryPatch patcher;
  FontData dict;
  dict.copy((const char*) dict_data.data(), dict_data.size());
  FontData patch;
  patch.copy((const char*) stream.compressed_data().data(), stream.compressed_data().size());

  Span<const char> expected_char((const char*) expected.data(), expected.size());

  FontData uncompressed;
  ASSERT_EQ(patcher.Patch(dict, patch, &uncompressed), StatusCode::kOk);
  EXPECT_EQ(Span<const char>(uncompressed),
            expected_char);
}

TEST_F(BrotliStreamTest, InsertUncompressed) {
  BrotliStream stream(22);
  uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
  stream.insert_uncompressed(data);
  stream.end_stream();

  CheckDecompressesTo(stream, data);
}

TEST_F(BrotliStreamTest, InsertUncompressedMultiple) {
  BrotliStream stream(22);
  uint8_t data_1[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
  uint8_t data_2[] = {'t', 'e', 's', 't'};
  stream.insert_uncompressed(data_1);
  stream.insert_uncompressed(data_2);
  stream.end_stream();

  CheckDecompressesTo(stream,
                      {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', 't', 'e', 's', 't'});
}

TEST_F(BrotliStreamTest, InsertUncompressedLarge) {
  BrotliStream stream(22);
  std::vector<uint8_t> data;
  data.resize(26777216);
  data[100] = 123;
  data[25000000] = 45;
  stream.insert_uncompressed(data);
  stream.end_stream();

  CheckDecompressesTo(stream, data);
}


TEST_F(BrotliStreamTest, InsertFromDictionary) {
  BrotliStream stream(22, 11);
  uint8_t dict_data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};

  stream.insert_from_dictionary(1, 4);
  stream.insert_from_dictionary(6, 3);
  stream.end_stream();

  uint8_t expected[] = {'e', 'l', 'l', 'o', 'w', 'o', 'r'};
  CheckDecompressesTo(stream, expected, dict_data);
}

TEST_F(BrotliStreamTest, InsertMixed) {
  BrotliStream stream(22, 11);
  uint8_t dict_data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
  uint8_t data[] = {'1', '2', '3'};

  stream.insert_from_dictionary(1, 4);
  stream.insert_uncompressed(data);
  stream.insert_from_dictionary(6, 3);
  stream.end_stream();

  uint8_t expected[] = {'e', 'l', 'l', 'o', '1', '2', '3', 'w', 'o', 'r'};
  CheckDecompressesTo(stream, expected, dict_data);
}


}  // namespace patch_subset
