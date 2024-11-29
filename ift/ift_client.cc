#include "ift/ift_client.h"

#include <iterator>
#include <sstream>

#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/binary_patch.h"
#include "common/font_data.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"
#include "uritemplate.hpp"
#include "base32_hex.hpp"

using absl::btree_set;
using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using common::AxisRange;
using common::BinaryPatch;
using common::FontData;
using ift::proto::DEFAULT_ENCODING;
using ift::proto::TABLE_KEYED_FULL;
using ift::proto::TABLE_KEYED_PARTIAL;
using ift::proto::GLYPH_KEYED;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using uritemplatecpp::UriTemplate;
using cppcodec::base32_hex;

namespace ift {

StatusOr<IFTClient> IFTClient::NewClient(common::FontData&& font) {
  IFTClient client;
  auto s = client.SetFont(std::move(font));
  if (!s.ok()) {
    return s;
  }

  return client;
}

StatusOr<std::string> IFTClient::UrlForEntry(uint32_t entry_idx) {
  if (!ift_table_) {
    return absl::FailedPreconditionError(
        "There are no entries to get URLs from.");
  }

  if (entry_idx >= ift_table_->GetPatchMap().GetEntries().size()) {
    return absl::InvalidArgumentError(StrCat("Invalid entry_idx, ", entry_idx));
  }

  const auto& entry = ift_table_->GetPatchMap().GetEntries().at(entry_idx);
  absl::string_view url_template = entry.extension_entry
                                       ? ift_table_->GetExtensionUrlTemplate()
                                       : ift_table_->GetUrlTemplate();

  return PatchToUrl(url_template, entry.patch_index);
}

std::string IFTClient::PatchToUrl(absl::string_view url_template,
                                  uint32_t patch_idx) {

  uint8_t bytes[4];
  bytes[0] = (patch_idx >> 24) & 0x000000FFu;
  bytes[1] = (patch_idx >> 16) & 0x000000FFu;
  bytes[2] = (patch_idx >> 8) & 0x000000FFu;
  bytes[3] = patch_idx & 0x000000FFu;

  size_t start = 0;
  while (start < 3 && !bytes[start]) {
    start++;
  }
  
  std::string result = base32_hex::encode(bytes + start, 4 - start);
  result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
        return ch != '=';
  }).base(), result.end());

  std::string url_template_copy {url_template};
  UriTemplate uri(url_template_copy);
  uri.set("id", result);

  if (result.size() >= 1) {
    uri.set("d1", result.substr(result.size() - 1, 1));
  } else {
    uri.set("d1", "_");
  }

  if (result.size() >= 2) {
    uri.set("d2", result.substr(result.size() - 2, 1));
  } else {
    uri.set("d2", "_");
  }

  if (result.size() >= 3) {
    uri.set("d3", result.substr(result.size() - 3, 1));
  } else {
    uri.set("d3", "_");
  }

  if (result.size() >= 4) {
    uri.set("d4", result.substr(result.size() - 4, 1));
  } else {
    uri.set("d4", "_");
  }

  // TODO(garretrieger): add additional variable id64

	return uri.build();
}

flat_hash_set<std::string> IFTClient::PatchesNeeded() const {
  flat_hash_set<std::string> result;
  for (const auto& [url, info] : pending_patches_) {
    if (!info.data.has_value()) {
      result.insert(url);
    }
  }
  return result;
}

void IFTClient::AddDesiredCodepoints(
    const absl::flat_hash_set<uint32_t>& codepoints) {
  std::copy(codepoints.begin(), codepoints.end(),
            std::inserter(target_codepoints_, target_codepoints_.begin()));
}

void IFTClient::AddDesiredFeatures(
    const absl::flat_hash_set<hb_tag_t>& features) {
  std::copy(features.begin(), features.end(),
            std::inserter(target_features_, target_features_.begin()));
}

Status IFTClient::AddDesiredDesignSpace(hb_tag_t axis_tag, float start,
                                        float end) {
  auto existing = design_space_.find(axis_tag);
  if (existing != design_space_.end()) {
    // If a range is already set then form a superset range that covers both.
    start = std::min(start, existing->second.start());
    end = std::max(end, existing->second.end());
  }

  auto range = AxisRange::Range(start, end);
  if (!range.ok()) {
    return range.status();
  }

  design_space_[axis_tag] = *range;
  return absl::OkStatus();
}

void IFTClient::AddPatch(absl::string_view id, const FontData& font_data) {
  auto existing = pending_patches_.find(id);
  if (existing == pending_patches_.end()) {
    // this is not a patch we are waiting for, ignore it.
    return;
  }

  if (existing->second.data.has_value()) {
    // this patch has already been supplied.
    return;
  }

  if (missing_patch_count_ > 0) {
    missing_patch_count_--;
  }
  existing->second.data = FontData();
  existing->second.data->shallow_copy(font_data);
}

StatusOr<IFTClient::State> IFTClient::Process() {
  if (!status_.ok()) {
    return status_;
  }

  if (missing_patch_count_ > 0) {
    return NEEDS_PATCHES;
  }

  if (pending_patches_.empty()) {
    // Check if any more patches are needed.
    return ComputeOutstandingPatches();
  }

  // - When applying patches apply any dependent patches first.
  // - There should only ever be one dependent patch in pending_pathches_.
  // - If there are more that's an error.
  // - Dependent patch applications may add more outstanding patches.
  //   Return early if there are new outstanding patches.
  // - Otherwise apply all pending independent patches.
  std::optional<std::string> dependent_patch;
  PatchEncoding dependent_patch_encoding = DEFAULT_ENCODING;
  std::vector<FontData> dependent_patch_data;
  for (const auto& [url, info] : pending_patches_) {
    if (!info.data.has_value()) {
      status_ = absl::FailedPreconditionError(
          "Missing patch data, should not happen.");
    }

    if (!PatchMap::IsDependent(info.encoding)) {
      continue;
    }

    if (dependent_patch) {
      status_ = absl::InternalError(StrCat(
          "Multiple dependent patches are pending. A max of one is allowed: ",
          *dependent_patch, url));
      return status_;
    }

    dependent_patch = url;
    dependent_patch_encoding = info.encoding;
    dependent_patch_data.resize(1);
    dependent_patch_data[0].shallow_copy(*info.data);
  }

  if (dependent_patch) {
    auto s = ApplyPatches(dependent_patch_data, dependent_patch_encoding);
    if (!s.ok()) {
      status_ = s;
      return s;
    }
    pending_patches_.erase(*dependent_patch);

    auto new_state = ComputeOutstandingPatches();
    if (!new_state.ok() || *new_state == NEEDS_PATCHES) {
      return new_state;
    }
  }

  std::vector<std::string> urls;
  std::vector<FontData> data;
  for (const auto& [url, info] : pending_patches_) {
    if (info.encoding != GLYPH_KEYED) {
      continue;
    }

    if (!info.data.has_value()) {
      continue;
    }

    FontData new_data;
    new_data.shallow_copy(*info.data);
    urls.push_back(url);
    data.push_back(std::move(new_data));
  }

  if (!urls.empty()) {
    auto s = ApplyPatches(data, GLYPH_KEYED);
    if (!s.ok()) {
      status_ = s;
      return s;
    }
    for (const auto& url : urls) {
      pending_patches_.erase(url);
    }
  }

  if (!pending_patches_.empty()) {
    status_ = absl::InternalError(
        "Pending patches remain after processing finished.");
    return status_;
  }

  return READY;
}

StatusOr<IFTClient::State> IFTClient::ComputeOutstandingPatches() {
  if (!status_.ok()) {
    return status_;
  }

  if (!ift_table_) {
    // There's no mapping table left, so no entries to add.
    status_ = absl::OkStatus();
    return READY;
  }

  // Patch matching algorithm works like this:
  // 1. Identify all patches listed in the IFT table which intersect the input
  //    codepoints.
  // 2. Keep all of those that are independent.
  // 3. Of the matched dependent patches, keep only one. Select the patch with
  //    the largest coverage.
  absl::flat_hash_set<uint32_t> candidate_indices = FindCandidateIndices();
  absl::flat_hash_set<uint32_t> independent_entry_indices;
  // keep dep entries sorted so that ties during single entry selection
  // are broken consistently.
  absl::btree_set<uint32_t> dependent_entry_indices;
  IntersectingEntries(candidate_indices, independent_entry_indices,
                      dependent_entry_indices);

  if (!dependent_entry_indices.empty()) {
    // Pick at most one dependent patches to keep.
    independent_entry_indices.insert(
        SelectDependentEntry(dependent_entry_indices));
  }

  flat_hash_set<std::string> new_urls;

  for (uint32_t entry_index : independent_entry_indices) {
    const PatchMap::Entry& entry =
        ift_table_->GetPatchMap().GetEntries().at(entry_index);

    PatchInfo info;
    info.encoding = entry.encoding;
    auto url = UrlForEntry(entry_index);
    if (!url.ok()) {
      status_ = url.status();
      return status_;
    }

    new_urls.insert(*url);
    auto [it, was_inserted] =
        pending_patches_.insert(std::pair(*url, std::move(info)));

    if (!was_inserted && it->second.encoding != entry.encoding) {
      status_ = absl::InternalError(
          StrCat("Invalid IFT table. patch ,  ", entry.patch_index,
                 ", has conflicting encoding types: ", entry.encoding,
                 " != ", it->second.encoding));
      return status_;
    }
  }

  for (auto it = pending_patches_.begin(); it != pending_patches_.end();) {
    const std::string& url = it->first;
    if (new_urls.contains(url)) {
      ++it;
      continue;
    }

    // Clean out entries which are no longer needed.
    pending_patches_.erase(it++);
  }

  missing_patch_count_ = MissingPatchCount();
  status_ = absl::OkStatus();
  return missing_patch_count_ > 0 ? NEEDS_PATCHES : READY;
}

uint32_t IFTClient::MissingPatchCount() const {
  uint32_t count = 0;
  for (const auto& [url, info] : pending_patches_) {
    if (!info.data.has_value()) {
      count++;
    }
  }
  return count;
}

Status IFTClient::ApplyPatches(const std::vector<FontData>& patches,
                               PatchEncoding encoding) {
  auto patcher = PatcherFor(encoding);
  if (!patcher.ok()) {
    return patcher.status();
  }

  FontData result;
  Status s = (*patcher)->Patch(font_, patches, &result);
  if (!s.ok()) {
    return s;
  }

  return SetFont(std::move(result));
}

StatusOr<const BinaryPatch*> IFTClient::PatcherFor(
    ift::proto::PatchEncoding encoding) const {
  switch (encoding) {
    case TABLE_KEYED_FULL:
    case TABLE_KEYED_PARTIAL:
      return per_table_binary_patch_.get();
    case GLYPH_KEYED:
      return iftb_binary_patch_.get();
    default:
      std::stringstream message;
      message << "Patch encoding " << encoding << " is not implemented.";
      return absl::UnimplementedError(message.str());
  }
}

Status IFTClient::SetFont(common::FontData&& new_font) {
  hb_face_t* face = new_font.reference_face();

  auto table = IFTTable::FromFont(face);
  if (table.ok()) {
    ift_table_ = std::move(*table);
  } else if (absl::IsNotFound(table.status())) {
    ift_table_.reset();
  } else if (!table.ok()) {
    hb_face_destroy(face);
    return table.status();
  }

  font_ = std::move(new_font);
  hb_face_destroy(face_);
  face_ = face;

  UpdateIndex();
  return absl::OkStatus();
}

flat_hash_set<uint32_t> IFTClient::FindCandidateIndices() const {
  flat_hash_set<uint32_t> candidate_indices;
  for (uint32_t cp : target_codepoints_) {
    auto indices = codepoint_to_entries_index_.find(cp);
    if (indices != codepoint_to_entries_index_.end()) {
      std::copy(indices->second.begin(), indices->second.end(),
                std::inserter(candidate_indices, candidate_indices.begin()));
    }
  }

  auto indices = codepoint_to_entries_index_.find(ALL_CODEPOINTS);
  if (indices != codepoint_to_entries_index_.end()) {
    std::copy(indices->second.begin(), indices->second.end(),
              std::inserter(candidate_indices, candidate_indices.begin()));
  }

  return candidate_indices;
}

void IFTClient::IntersectingEntries(
    const absl::flat_hash_set<uint32_t>& candidate_indices,
    absl::flat_hash_set<uint32_t>& independent_entry_indices,
    absl::btree_set<uint32_t>& dependent_entry_indices) {
  for (uint32_t index : candidate_indices) {
    const auto& entry = ift_table_->GetPatchMap().GetEntries().at(index);

    if (!entry.coverage.Intersects(target_codepoints_, target_features_,
                                   design_space_)) {
      continue;
    }

    if (entry.IsDependent()) {
      dependent_entry_indices.insert(index);
    } else {
      independent_entry_indices.insert(index);
    }
  }
}

int intersection_size(const flat_hash_set<uint32_t>* a,
                      const flat_hash_set<uint32_t>* b) {
  const flat_hash_set<uint32_t>* smaller = a->size() < b->size() ? a : b;
  const flat_hash_set<uint32_t>* larger = a->size() < b->size() ? b : a;

  uint32_t count = 0;
  for (uint32_t v : *smaller) {
    if (larger->contains(v)) {
      count++;
    }
  }
  return count;
}

uint32_t IFTClient::SelectDependentEntry(
    const absl::btree_set<uint32_t>& dependent_entry_indices) {
  // TODO(garretrieger): merge coverages when multiple entries have the same
  // patch index.
  //
  // Algorithm:
  // - Select the entry that has the highest intersecting codepoint coverage.
  // - Breaking ties:
  //   1. Prefer the entry that also has interesting design space expansion.
  //   2. Prefer the entry with a smaller overall codepoint coverage.
  uint32_t selected_entry_index;
  int64_t max_intersection = -1;
  uint32_t min_size = (uint32_t)-1;
  bool selected_entry_has_design_space_expansion = false;

  for (uint32_t entry_index : dependent_entry_indices) {
    const PatchMap::Entry& entry =
        ift_table_->GetPatchMap().GetEntries().at(entry_index);

    uint32_t intersection_count =
        intersection_size(&(entry.coverage.codepoints), &target_codepoints_);
    uint32_t size = entry.coverage.codepoints.size();
    bool has_design_space_expansion = !entry.coverage.design_space.empty();

    if (intersection_count > max_intersection) {
      max_intersection = intersection_count;
      min_size = size;
      selected_entry_index = entry_index;
      selected_entry_has_design_space_expansion = has_design_space_expansion;
      continue;
    }

    if (intersection_count < max_intersection) {
      // Not a candidate.
      continue;
    }

    // Break ties first to entries that have design space expansion
    if (!selected_entry_has_design_space_expansion &&
        has_design_space_expansion) {
      min_size = size;
      selected_entry_has_design_space_expansion = has_design_space_expansion;
      selected_entry_index = entry_index;
      continue;
    }

    // Then to the entries with lower total codepoint coverage
    if (intersection_count == max_intersection && size < min_size) {
      min_size = size;
      selected_entry_has_design_space_expansion = has_design_space_expansion;
      selected_entry_index = entry_index;
      continue;
    }
  }
  return selected_entry_index;
}

void IFTClient::UpdateIndex() {
  codepoint_to_entries_index_.clear();
  if (!ift_table_) {
    return;
  }

  uint32_t entry_index = 0;
  for (const auto& e : ift_table_->GetPatchMap().GetEntries()) {
    if (!e.coverage.codepoints.empty()) {
      for (uint32_t cp : e.coverage.codepoints) {
        codepoint_to_entries_index_[cp].push_back(entry_index);
      }
    } else {
      codepoint_to_entries_index_[ALL_CODEPOINTS].push_back(entry_index);
    }

    entry_index++;
  }
}

}  // namespace ift
