#ifndef IFT_IFTB_BINARY_PATCH_H_
#define IFT_IFTB_BINARY_PATCH_H_

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "common/brotli_binary_diff.h"
#include "common/compat_id.h"
#include "common/font_data.h"

namespace ift {

/* Applies one or more IFTB chunk file patches. */
class GlyphKeyedDiff {
 public:
  static absl::StatusOr<absl::flat_hash_set<uint32_t>> GidsInIftbPatch(
      const common::FontData& patch);

  static absl::StatusOr<common::CompatId> IdInIftbPatch(
      const common::FontData& patch);

  GlyphKeyedDiff(const common::FontData& font, common::CompatId base_compat_id,
                 absl::flat_hash_set<hb_tag_t> included_tags)
      : font_(font), base_compat_id_(base_compat_id), tags_(included_tags) {}

  absl::StatusOr<common::FontData> CreatePatch(
      const absl::btree_set<uint32_t>& gids) const;

 private:
  absl::StatusOr<common::FontData> CreateDataStream(
      const absl::btree_set<uint32_t>& gids, bool u16_gids) const;

  const common::FontData& font_;
  common::CompatId base_compat_id_;
  absl::flat_hash_set<hb_tag_t> tags_;
  common::BrotliBinaryDiff brotli_diff_;
};

}  // namespace ift

#endif  // IFT_IFTB_BINARY_PATCH_H_
