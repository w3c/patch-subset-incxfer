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

StatusOr<PatchMap::Coverage> PatchMap::Coverage::FromProto(
    const ift::proto::SubsetMapping& mapping) {
  uint32_t bias = mapping.bias();
  hb_set_unique_ptr codepoints = make_hb_set();
  auto s = SparseBitSet::Decode(mapping.codepoint_set(), codepoints.get());
  if (!s.ok()) {
    return s;
  }

  Coverage coverage;
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(codepoints.get(), &cp)) {
    uint32_t actual_cp = cp + bias;
    coverage.codepoints.insert(actual_cp);
  }

  for (uint32_t feature_index : mapping.feature_index()) {
    coverage.features.insert(IndexToFeatureTag(feature_index));
  }

  for (const auto& entry : mapping.design_space()) {
    hb_tag_t tag = entry.first;
    auto range = PatchMap::FromProto(entry.second);
    if (!range.ok()) {
      return range.status();
    }

    coverage.design_space[tag] = *range;
  }

  return coverage;
}

void PatchMap::Coverage::ToProto(SubsetMapping* out) const {
  if (!codepoints.empty()) {
    hb_set_unique_ptr set = make_hb_set();
    for (uint32_t cp : codepoints) {
      hb_set_add(set.get(), cp);
    }

    uint32_t bias = hb_set_get_min(set.get());
    hb_set_clear(set.get());
    for (uint32_t cp : codepoints) {
      hb_set_add(set.get(), cp - bias);
    }

    std::string encoded = common::SparseBitSet::Encode(*set);
    out->set_bias(bias);
    out->set_codepoint_set(encoded);
  }

  btree_set<hb_tag_t> sorted_features;
  std::copy(features.begin(), features.end(),
            std::inserter(sorted_features, sorted_features.begin()));

  for (hb_tag_t feature_tag : sorted_features) {
    out->add_feature_index(FeatureTagToIndex(feature_tag));
  }

  for (const auto& [tag, range] : design_space) {
    PatchMap::ToProto(range, &(*out->mutable_design_space())[tag]);
  }
}

static bool sets_intersect(const flat_hash_set<uint32_t>& a,
                           const flat_hash_set<uint32_t>& b) {
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
    const flat_hash_set<hb_tag_t>& features_in,
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

StatusOr<PatchMap::Entry> PatchMap::Entry::FromProto(
    const ift::proto::SubsetMapping& mapping, uint32_t index,
    PatchEncoding enc) {
  auto c = Coverage::FromProto(mapping);
  if (!c.ok()) {
    return c.status();
  }

  Entry entry;
  entry.coverage = std::move(*c);
  entry.encoding = enc;
  entry.patch_index = index;
  return entry;
}

void PatchMap::Entry::ToProto(uint32_t last_patch_index,
                              ift::proto::PatchEncoding default_encoding,
                              SubsetMapping* out) const {
  coverage.ToProto(out);

  int32_t delta = ((int64_t)patch_index) - ((int64_t)last_patch_index) - 1;
  out->set_id_delta(delta);

  if (encoding != default_encoding) {
    out->set_patch_encoding(encoding);
  }
}

StatusOr<PatchMap> PatchMap::FromProto(const IFT& ift_proto) {
  PatchMap map;
  auto s = map.AddFromProto(ift_proto);
  if (!s.ok()) {
    return s;
  }
  return map;
}

Status PatchMap::AddFromProto(const IFT& ift_proto, bool is_extension_table) {
  PatchEncoding default_encoding = ift_proto.default_patch_encoding();
  int32_t id = 0;
  for (const auto& m : ift_proto.subset_mapping()) {
    id += m.id_delta() + 1;
    uint32_t patch_idx = id;
    PatchEncoding encoding = m.patch_encoding();
    if (encoding == DEFAULT_ENCODING) {
      encoding = default_encoding;
    }

    auto e = Entry::FromProto(m, patch_idx, encoding);
    if (!e.ok()) {
      return e.status();
    }

    e->extension_entry = is_extension_table;
    entries_.push_back(std::move(*e));
  }

  return absl::OkStatus();
}

void PatchMap::AddToProto(IFT& ift_proto, bool extension_entries) const {
  PatchEncoding default_encoding = ift_proto.default_patch_encoding();
  uint32_t last_patch_index = 0;
  for (const Entry& e : entries_) {
    if (e.extension_entry != extension_entries) {
      continue;
    }

    if (default_encoding == DEFAULT_ENCODING) {
      // Assign an encoding if one has not been picked yet.
      // This could be made smarter by picking the most commonly
      // used encoding.
      default_encoding = e.encoding;
      ift_proto.set_default_patch_encoding(e.encoding);
    }

    auto* m = ift_proto.add_subset_mapping();
    e.ToProto(last_patch_index, default_encoding, m);
    last_patch_index = e.patch_index;
  }
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
    if (e.extension_entry) {
      *os << "  ExtEntry {";
    } else {
      *os << "  Entry {";
    }
    PrintTo(e, os);
    *os << "}," << std::endl;
  }
  *os << "]";
}

Span<const PatchMap::Entry> PatchMap::GetEntries() const { return entries_; }

void PatchMap::AddEntry(const PatchMap::Coverage& coverage,
                        uint32_t patch_index, PatchEncoding encoding,
                        bool is_extension) {
  Entry e;
  e.coverage = coverage;
  e.patch_index = patch_index;
  e.encoding = encoding;
  e.extension_entry = is_extension;
  entries_.push_back(std::move(e));
}

PatchMap::Modification PatchMap::RemoveEntries(uint32_t patch_index) {
  bool modified_ext = false;
  bool modified_main = false;
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->patch_index == patch_index) {
      modified_main = modified_main || !it->extension_entry;
      modified_ext = modified_ext || it->extension_entry;
      entries_.erase(it);
      continue;
    }
    ++it;
  }

  if (modified_ext && modified_main) {
    return MODIFIED_BOTH;
  }

  if (modified_ext) {
    return MODIFIED_EXTENSION;
  }

  if (modified_main) {
    return MODIFIED_MAIN;
  }

  return MODIFIED_NEITHER;
}

}  // namespace ift::proto
