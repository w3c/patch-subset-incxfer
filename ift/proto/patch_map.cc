#include "ift/proto/patch_map.h"

#include <iterator>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/font_helper.h"
#include "common/hb_set_unique_ptr.h"
#include "common/sparse_bit_set.h"
#include "ift/feature_registry/feature_registry.h"
#include "ift/proto/IFT.pb.h"

using absl::btree_set;
using absl::flat_hash_set;
using absl::Span;
using absl::Status;
using absl::StatusOr;
using common::FontHelper;
using common::hb_set_unique_ptr;
using common::make_hb_set;
using common::SparseBitSet;
using ift::feature_registry::FeatureTagToIndex;
using ift::feature_registry::IndexToFeatureTag;

namespace ift::proto {

template<typename S>
static bool sets_intersect(const S& a,
                           const S& b) {
  bool a_smaller = a.size() < b.size();
  const auto& smaller = a_smaller ? a : b;
  const auto& larger = a_smaller ? b : a;
  for (uint32_t v : smaller) {
    if (larger.contains(v)) {
      return true;
    }
  }
  return false;
}

bool PatchMap::Coverage::Intersects(
    const flat_hash_set<uint32_t>& codepoints_in,
    const btree_set<hb_tag_t>& features_in,
    const absl::flat_hash_map<hb_tag_t, common::AxisRange>& design_space_in)
    const {
  // If an input set is unspecified (empty), it is considered to not match the
  // corresponding coverage set if that set is specified (not empty).
  if (codepoints_in.empty() && !codepoints.empty()) {
    return false;
  }

  if (features_in.empty() && !features.empty()) {
    return false;
  }

  if (design_space_in.empty() && !design_space.empty()) {
    return false;
  }

  // Otherwise, if the coverage set is unspecified (empty) it is considered to
  // match all things so only check for intersections if the input and coverage
  // sets are non-empty.
  if (!codepoints_in.empty() && !codepoints.empty()) {
    if (!sets_intersect(codepoints_in, codepoints)) {
      return false;
    }
  }

  if (!features_in.empty() && !features.empty()) {
    if (!sets_intersect(features_in, features)) {
      return false;
    }
  }

  if (!design_space_in.empty() && !design_space.empty()) {
    bool has_intersection = false;
    for (const auto& [tag, range] : design_space) {
      auto e = design_space_in.find(tag);
      if (e != design_space_in.end() && range.Intersects(e->second)) {
        has_intersection = true;
        break;
      }
    }
    if (!has_intersection) {
      return false;
    }
  }

  return true;
}

void PrintTo(const PatchMap::Coverage& coverage, std::ostream* os) {
  absl::btree_set<uint32_t> sorted_codepoints;
  std::copy(coverage.codepoints.begin(), coverage.codepoints.end(),
            std::inserter(sorted_codepoints, sorted_codepoints.begin()));

  if (!coverage.features.empty() || !coverage.design_space.empty()) {
    *os << "{";
  }
  *os << "{";
  for (auto it = sorted_codepoints.begin(); it != sorted_codepoints.end();
       it++) {
    *os << *it;
    auto next = it;
    if (++next != sorted_codepoints.end()) {
      *os << ", ";
    }
  }
  *os << "}";
  if (coverage.features.empty() && coverage.design_space.empty()) {
    return;
  }

  if (!coverage.features.empty()) {
    *os << ", {";
    absl::btree_set<hb_tag_t> sorted_features;
    std::copy(coverage.features.begin(), coverage.features.end(),
              std::inserter(sorted_features, sorted_features.begin()));

    for (auto it = sorted_features.begin(); it != sorted_features.end(); it++) {
      std::string tag;
      tag.resize(5);
      snprintf(tag.data(), 5, "%c%c%c%c", HB_UNTAG(*it));
      tag.resize(4);
      *os << tag;
      auto next = it;
      if (++next != sorted_features.end()) {
        *os << ", ";
      }
    }
    *os << "}";
  }

  *os << ", {";
  if (!coverage.design_space.empty()) {
    for (const auto& [tag, range] : coverage.design_space) {
      *os << FontHelper::ToString(tag) << ": ";
      PrintTo(range, os);
      *os << ", ";
    }
  }

  *os << "}";
}

void PrintTo(const PatchMap::Entry& entry, std::ostream* os) {
  PrintTo(entry.coverage, os);
  *os << ", " << entry.patch_index << ", " << entry.encoding;
}

void PrintTo(const PatchMap& map, std::ostream* os) {
  *os << "[" << std::endl;
  for (const auto& e : map.entries_) {
    *os << "  Entry {";
    PrintTo(e, os);
    *os << "}," << std::endl;
  }
  *os << "]";
}

Span<const PatchMap::Entry> PatchMap::GetEntries() const { return entries_; }

void PatchMap::AddEntry(const PatchMap::Coverage& coverage,
                        uint32_t patch_index, PatchEncoding encoding) {
  Entry e;
  e.coverage = coverage;
  e.patch_index = patch_index;
  e.encoding = encoding;
  entries_.push_back(std::move(e));
}

}  // namespace ift::proto
