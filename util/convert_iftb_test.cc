#include "util/convert_iftb.h"

#include <set>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "common/sparse_bit_set.h"
#include "gtest/gtest.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"

using absl::flat_hash_set;
using ::common::SparseBitSet;
using ::ift::proto::IFT;
using ::ift::proto::SubsetMapping;

namespace util {

class ConvertIftbTest : public ::testing::Test {
 protected:
  ConvertIftbTest() {
    hb_blob_t* blob = hb_blob_create_from_file_or_fail(
        "util/testdata/convert-iftb-sample.txt");
    assert(blob);

    unsigned length;
    const char* data = hb_blob_get_data(blob, &length);
    sample_input = std::string(data, length);
    hb_blob_destroy(blob);

    blob = hb_blob_create_from_file_or_fail(
        "util/testdata/Roboto-Regular.Awesome.ttf");
    assert(blob);

    face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
  }

  ~ConvertIftbTest() { hb_face_destroy(face); }

  std::string sample_input;
  hb_face_t* face;
};

TEST_F(ConvertIftbTest, BasicConversion) {
  auto ift = convert_iftb(sample_input, face);
  ASSERT_TRUE(ift.ok()) << ift.status();

  ASSERT_EQ(ift->GetUrlTemplate(), "./Roboto-Regular_iftb/$3/chunk$3$2$1.br");
  ASSERT_EQ(ift->GetPatchMap().GetEntries().size(), 2);

  flat_hash_set<uint32_t> expected1{{0x41, 0x65, 0x6d}};
  ASSERT_EQ(ift->GetPatchMap().GetEntries().at(0).coverage.codepoints,
            expected1);
  ASSERT_EQ(ift->GetPatchMap().GetEntries().at(0).patch_index, 1);

  flat_hash_set<uint32_t> expected2{{0x6f, 0x77, 0x80}};
  ASSERT_EQ(ift->GetPatchMap().GetEntries().at(1).coverage.codepoints,
            expected2);
  ASSERT_EQ(ift->GetPatchMap().GetEntries().at(1).patch_index, 2);
}

}  // namespace util
