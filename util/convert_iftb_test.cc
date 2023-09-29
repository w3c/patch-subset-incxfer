#include "util/convert_iftb.h"

#include <set>
#include <string>

#include "gtest/gtest.h"
#include "hb.h"
#include "patch_subset/proto/IFT.pb.h"
#include "patch_subset/sparse_bit_set.h"

using ::patch_subset::SparseBitSet;

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

std::set<uint32_t> to_set(const SubsetMapping& mapping) {
  unsigned bias = mapping.bias();
  hb_set_t* set = hb_set_create();
  if (!SparseBitSet::Decode(mapping.codepoint_set(), set).ok()) {
    hb_set_destroy(set);
    return std::set<uint32_t>();
  }

  std::set<uint32_t> result;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(set, &cp)) {
    result.insert(bias + cp);
  }

  hb_set_destroy(set);
  return result;
}

TEST_F(ConvertIftbTest, BasicConversion) {
  IFT ift = convert_iftb(sample_input, face);

  ASSERT_EQ(ift.url_template(), "./Roboto-Regular_iftb/$3/chunk$3$2$1.br");
  ASSERT_EQ(ift.subset_mapping_size(), 2);

  std::set<uint32_t> expected1{{0x41, 0x65, 0x6d}};
  ASSERT_EQ(to_set(ift.subset_mapping(0)), expected1);
  ASSERT_EQ(ift.subset_mapping(0).id(), 1);

  std::set<uint32_t> expected2{{0x6f, 0x77, 0x80}};
  ASSERT_EQ(to_set(ift.subset_mapping(1)), expected2);
  ASSERT_EQ(ift.subset_mapping(1).id(), 2);
}

}  // namespace util
