#ifndef IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_
#define IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "patch_subset/binary_diff.h"
#include "patch_subset/brotli_binary_diff.h"
#include "patch_subset/font_data.h"

namespace ift {

/* Creates a per table brotli binary diff of two fonts. */
class PerTableBrotliBinaryDiff : public patch_subset::BinaryDiff {
 public:
  PerTableBrotliBinaryDiff() {}

  PerTableBrotliBinaryDiff(absl::Span<const std::string> target_tags) {
    std::copy(target_tags.begin(), target_tags.end(),
              std::inserter(target_tags_, target_tags_.begin()));
  }

  absl::Status Diff(const patch_subset::FontData& font_base,
                    const patch_subset::FontData& font_derived,
                    patch_subset::FontData* patch /* OUT */) const override;

 private:
  void AddAllMatching(const absl::flat_hash_set<uint32_t>& tags,
                      absl::btree_set<std::string>& result) const;
  absl::btree_set<std::string> TagsToDiff(
      const absl::flat_hash_set<uint32_t>& before,
      const absl::flat_hash_set<uint32_t>& after) const;

  patch_subset::BrotliBinaryDiff binary_diff_;
  absl::btree_set<std::string> target_tags_;
};

}  // namespace ift

#endif  // IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_
