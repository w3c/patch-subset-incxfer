#include "patch_subset/py/patch_subset_session.h"

#include "gtest/gtest.h"

class PatchSubsetSessionTest : public ::testing::Test {
 protected:
  PatchSubsetSessionTest() {}
};

TEST_F(PatchSubsetSessionTest, SessionWithRemap) {
  PatchSubsetSession* session = PatchSubsetSession_new(
      "./patch_subset/testdata/", "Roboto-Regular.ttf", true, 0, 0.0f);

  uint32_t codepoints_1[2] = {0x61, 0x62};
  PatchSubsetSession_extend(session, codepoints_1, 2);

  uint32_t codepoints_2[4] = {0x61, 0x62, 0x63, 0x64};
  PatchSubsetSession_extend(session, codepoints_2, 4);

  uint32_t codepoints_3[1] = {0xAFFF};
  PatchSubsetSession_extend(session, codepoints_3, 1);

  PatchSubsetSession_delete(session);
}
