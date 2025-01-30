#include "ift/encoder/glyph_segmentation.h"

#include "common/font_data.h"
#include "gtest/gtest.h"

using common::FontData;
using common::hb_face_unique_ptr;
using common::make_hb_face;

namespace ift::encoder {

class GlyphSegmentationTest : public ::testing::Test {
 protected:
  GlyphSegmentationTest() : roboto(make_hb_face(nullptr)) {
    roboto = from_file("common/testdata/Roboto-Regular.ttf");
  }

  hb_face_unique_ptr from_file(const char* filename) {
    hb_blob_t* blob = hb_blob_create_from_file_or_fail(filename);
    if (!blob) {
      assert(false);
    }
    FontData result(blob);
    hb_blob_destroy(blob);
    return result.face();
  }

  hb_face_unique_ptr roboto;
};

TEST_F(GlyphSegmentationTest, SimpleSegmentation) {
  auto segmentation = GlyphSegmentation::CodepointToGlyphSegments(
      roboto.get(), {'a'}, {{'b'}, {'c'}});
  ASSERT_TRUE(segmentation.ok()) << segmentation.status();

  ASSERT_EQ(segmentation->ToString(),
            R"(initial font: { gid0, gid69 }
unmapped: {}
p0: { gid70 }
p1: { gid71 }
if (p0) then p0
if (p1) then p1
)");
}

TEST_F(GlyphSegmentationTest, AndCondition) {
  auto segmentation = GlyphSegmentation::CodepointToGlyphSegments(
      roboto.get(), {'a'}, {{'f'}, {'i'}});
  ASSERT_TRUE(segmentation.ok()) << segmentation.status();

  ASSERT_EQ(segmentation->ToString(),
            R"(initial font: { gid0, gid69 }
unmapped: {}
p0: { gid74 }
p1: { gid77 }
p2: { gid444, gid446 }
if (p0) then p0
if (p1) then p1
if (p0 AND p1) then p2
)");
}

TEST_F(GlyphSegmentationTest, OrCondition) {
  auto segmentation = GlyphSegmentation::CodepointToGlyphSegments(
      roboto.get(), {'a'}, {{0xc1}, {0x106}});
  ASSERT_TRUE(segmentation.ok()) << segmentation.status();

  ASSERT_EQ(segmentation->ToString(),
            R"(initial font: { gid0, gid69 }
unmapped: {}
p0: { gid37, gid640 }
p1: { gid39, gid700 }
p2: { gid117 }
if (p0) then p0
if (p1) then p1
if ((p0 OR p1)) then p2
)");
}

TEST_F(GlyphSegmentationTest, MixedAndOr) {
  auto segmentation = GlyphSegmentation::CodepointToGlyphSegments(
      roboto.get(), {'a'}, {{'f', 0xc1}, {'i', 0x106}});
  ASSERT_TRUE(segmentation.ok()) << segmentation.status();

  ASSERT_EQ(segmentation->ToString(),
            R"(initial font: { gid0, gid69 }
unmapped: {}
p0: { gid37, gid74, gid640 }
p1: { gid39, gid77, gid700 }
p2: { gid444, gid446 }
p3: { gid117 }
if (p0) then p0
if (p1) then p1
if (p0 AND p1) then p2
if ((p0 OR p1)) then p3
)");
}

// TODO(garretrieger): test that results in unmapped glyphs.

}  // namespace ift::encoder