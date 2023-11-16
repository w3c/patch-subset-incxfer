#include "ift/proto/patch_map.h"

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using absl::Span;
using absl::Status;
using absl::StatusOr;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using patch_subset::SparseBitSet;

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

  return coverage;
}

void PatchMap::Coverage::ToProto(SubsetMapping* out) const {
  if (codepoints.empty()) {
    return;
  }

  hb_set_unique_ptr set = make_hb_set();
  for (uint32_t cp : codepoints) {
    hb_set_add(set.get(), cp);
  }

  uint32_t bias = hb_set_get_min(set.get());
  hb_set_clear(set.get());
  for (uint32_t cp : codepoints) {
    hb_set_add(set.get(), cp - bias);
  }

  std::string encoded = patch_subset::SparseBitSet::Encode(*set);
  out->set_bias(bias);
  out->set_codepoint_set(encoded);
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

    auto* m = ift_proto.add_subset_mapping();
    e.ToProto(last_patch_index, default_encoding, m);
    last_patch_index = e.patch_index;
  }
}

void PrintTo(const PatchMap::Coverage& coverage, std::ostream* os) {
  absl::btree_set<uint32_t> sorted_codepoints;
  std::copy(coverage.codepoints.begin(), coverage.codepoints.end(),
            std::inserter(sorted_codepoints, sorted_codepoints.begin()));

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
}

void PrintTo(const PatchMap::Entry& entry, std::ostream* os) {
  PrintTo(entry.coverage, os);
  *os << ", " << entry.patch_index << ", " << entry.encoding;
  if (entry.extension_entry) {
    *os << ", ext";
  }
}

void PrintTo(const PatchMap& map, std::ostream* os) {
  *os << "[" << std::endl;
  for (const auto& e : map.entries_) {
    *os << "  {";
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