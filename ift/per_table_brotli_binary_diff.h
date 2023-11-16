#ifndef IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_
#define IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_

#include <initializer_list>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "common/binary_diff.h"
#include "common/brotli_binary_diff.h"
#include "common/font_data.h"

namespace ift {

/* Creates a per table brotli binary diff of two fonts. */
class PerTableBrotliBinaryDiff : public common::BinaryDiff {
 public:
  PerTableBrotliBinaryDiff() {}

  PerTableBrotliBinaryDiff(std::initializer_list<const char*> excluded_tags)
      : excluded_tags_() {
    std::copy(excluded_tags.begin(), excluded_tags.end(),
              std::inserter(excluded_tags_, excluded_tags_.begin()));
  }

  absl::Status Diff(const common::FontData& font_base,
                    const common::FontData& font_derived,
                    common::FontData* patch /* OUT */) const override;

 private:
  void AddAllMatching(const absl::flat_hash_set<uint32_t>& tags,
                      absl::btree_set<std::string>& result) const;
  absl::btree_set<std::string> TagsToDiff(
      const absl::flat_hash_set<uint32_t>& before,
      const absl::flat_hash_set<uint32_t>& after) const;

  common::BrotliBinaryDiff binary_diff_;
  absl::btree_set<std::string> excluded_tags_;
};

}  // namespace ift

#endif  // IFT_PER_TABLE_BROTLI_BINARY_DIFF_H_
