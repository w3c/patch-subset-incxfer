#ifndef IFT_ENCODER_GLYPH_SEGMENTATION_H_
#define IFT_ENCODER_GLYPH_SEGMENTATION_H_

#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "hb.h"

namespace ift::encoder {

// TODO XXXX comments
// TODO XXXX tests
class GlyphSegmentation {
 public:
  typedef uint32_t segment_index_t;
  typedef uint32_t patch_id_t;
  typedef uint32_t glyph_id_t;

  class ActivationCondition {
   public:
    // TODO XXXX rename to patches
    static ActivationCondition and_segments(
        const absl::btree_set<patch_id_t>& ids, patch_id_t activated);
    static ActivationCondition or_segments(
        const absl::btree_set<patch_id_t>& ids, patch_id_t activated);

    const std::vector<absl::btree_set<patch_id_t>>& segment_sets() const {
      return segment_sets_;
    }

    patch_id_t activated() const { return activated_; }

   private:
    ActivationCondition() : segment_sets_(), activated_(0) {}

    // This condition is activated if every set of segment ids intersects the
    // input subset definition. ie. input subset def intersects {s_1, s_2} AND
    // input subset def intersects {...} AND ...
    //     which is effectively: (s_1 OR s_2) AND ...
    std::vector<absl::btree_set<patch_id_t>> segment_sets_;
    patch_id_t activated_;
  };

  static absl::StatusOr<GlyphSegmentation> CodepointToGlyphSegments(
      hb_face_t* face, absl::flat_hash_set<hb_codepoint_t> initial_segment,
      std::vector<absl::flat_hash_set<hb_codepoint_t>> codepoint_segments);

  // TODO(garretrieger): to string method to allow for easy test setup.

  const std::vector<ActivationCondition>& conditions() const {
    return conditions_;
  }

  const absl::btree_map<patch_id_t, absl::btree_set<glyph_id_t>>& gid_segments()
      const {
    return patches_;
  }

  const absl::btree_set<glyph_id_t>& unmapped_glyphs() const {
    return unmapped_glyphs_;
  };

  const absl::btree_set<glyph_id_t>& init_font_glyphs() const {
    return init_font_glyphs_;
  };

 private:
  // TODO XXXXX list of unmatched glyph ids
  absl::btree_set<glyph_id_t> init_font_glyphs_;
  absl::btree_set<glyph_id_t> unmapped_glyphs_;
  std::vector<ActivationCondition> conditions_;
  absl::btree_map<patch_id_t, absl::btree_set<glyph_id_t>> patches_;
};

}  // namespace ift::encoder

#endif  // IFT_ENCODER_GLYPH_SEGMENTATION_H_