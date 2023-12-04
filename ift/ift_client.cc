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
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::PER_TABLE_SHARED_BROTLI_ENCODING;
using ift::proto::SHARED_BROTLI_ENCODING;

namespace ift {

StatusOr<IFTClient> IFTClient::NewClient(common::FontData&& font) {
  IFTClient client;
  auto s = client.SetFont(std::move(font));
  if (!s.ok()) {
    return s;
  }

  return client;
}

std::string IFTClient::PatchToUrl(const std::string& url_template,
                                  uint32_t patch_idx) {
  constexpr int num_digits = 5;
  int hex_digits[num_digits];
  int base = 1;
  for (int i = 0; i < num_digits; i++) {
    hex_digits[i] = (patch_idx / base) % 16;
    base *= 16;
  }

  std::stringstream out;

  size_t i = 0;
  while (true) {
    size_t from = i;
    i = url_template.find("$", i);
    if (i == std::string::npos) {
      out << url_template.substr(from);
      break;
    }
    out << url_template.substr(from, i - from);

    i++;
    if (i == url_template.length()) {
      out << "$";
      break;
    }

    char c = url_template[i];
    if (c < 0x31 || c >= 0x31 + num_digits) {
      out << "$";
      continue;
    }

    int digit = c - 0x31;
    out << std::hex << hex_digits[digit];
    i++;
  }

  return out.str();
}

flat_hash_set<uint32_t> IFTClient::PatchesNeeded() const {
  return outstanding_patches_;
}

Status IFTClient::AddDesiredCodepoints(
    const absl::flat_hash_set<uint32_t>& codepoints) {
  if (!status_.ok()) {
    return status_;
  }

  std::copy(codepoints.begin(), codepoints.end(),
            std::inserter(target_codepoints_, target_codepoints_.begin()));

  // TODO(garretrieger): in some cases this may cause a needed patch
  // to be replaced. However, the client  may still have fetched
  // and added that replaced patch. Make sure the client supports this
  // situation seemlessly (ignoring the no longer needed patch and should
  // not attempt to apply it). Add a test in he integration tests which
  // exercises this case.
  auto s = ComputeOutstandingPatches();
  if (!s.ok()) {
    status_ = s;
  }
  return s;
}

Status IFTClient::AddDesiredFeatures(
    const absl::flat_hash_set<hb_tag_t>& features) {
  if (!status_.ok()) {
    return status_;
  }

  std::copy(features.begin(), features.end(),
            std::inserter(target_features_, target_features_.begin()));

  auto s = ComputeOutstandingPatches();
  if (!s.ok()) {
    status_ = s;
  }
  return s;
}

Status IFTClient::AddDesiredDesignSpace(hb_tag_t axis_tag, float start,
                                        float end) {
  if (!status_.ok()) {
    return status_;
  }

  auto range = AxisRange::Range(start, end);
  if (!range.ok()) {
    return range.status();
  }

  design_space_[axis_tag] = *range;

  auto s = ComputeOutstandingPatches();
  if (!s.ok()) {
    status_ = s;
  }
  return s;
}

void IFTClient::AddPatch(uint32_t id, const FontData& font_data) {
  outstanding_patches_.erase(id);
  pending_patches_[id].shallow_copy(font_data);
}

StatusOr<IFTClient::State> IFTClient::Process() {
  // TODO(garretrieger): add a helper which classifies patches as
  // dependent/independent instead of hardcoding here.
  if (!status_.ok()) {
    return status_;
  }

  if (!outstanding_patches_.empty()) {
    return NEEDS_PATCHES;
  }

  if (pending_patches_.empty()) {
    return READY;
  }

  // - When applying patches apply any dependent patches first.
  // - There should only ever be one dependent patch in pending_pathches_.
  // - If there are more that's an error.
  // - Dependent patch applications may add more outstanding patches.
  //   Return early if there are new outstanding patches.
  // - Otherwise apply all pending independent patches.
  std::optional<uint32_t> dependent_patch;
  PatchEncoding dependent_patch_encoding = DEFAULT_ENCODING;
  std::vector<FontData> dependent_patch_data;
  for (const auto& p : pending_patches_) {
    uint32_t id = p.first;
    const FontData& patch_data = p.second;

    auto it = patch_to_encoding_.find(id);
    if (it == patch_to_encoding_.end()) {
      status_ =
          absl::InternalError(StrCat("No encoding stored for patch ", id));
      return status_;
    }
    PatchEncoding encoding = it->second;
    if (!PatchMap::IsDependent(encoding)) {
      continue;
    }

    if (dependent_patch) {
      status_ = absl::InternalError(StrCat(
          "Multiple dependent patches are pending. A max of one is allowed: ",
          *dependent_patch, id));
      return status_;
    }

    dependent_patch = id;
    dependent_patch_encoding = encoding;
    dependent_patch_data.resize(1);
    dependent_patch_data[0].shallow_copy(patch_data);
  }

  if (dependent_patch) {
    auto s = ApplyPatches(dependent_patch_data, dependent_patch_encoding);
    if (!s.ok()) {
      status_ = s;
      return s;
    }
    pending_patches_.erase(*dependent_patch);

    s = ComputeOutstandingPatches();
    if (!s.ok()) {
      status_ = s;
      return s;
    }

    if (!outstanding_patches_.empty()) {
      return NEEDS_PATCHES;
    }
  }

  std::vector<uint32_t> indices;
  std::vector<FontData> data;
  for (const auto& p : pending_patches_) {
    uint32_t id = p.first;
    const FontData& patch_data = p.second;

    auto it = patch_to_encoding_.find(id);
    if (it == patch_to_encoding_.end()) {
      status_ =
          absl::InternalError(StrCat("No encoding stored for patch ", id));
      return status_;
    }
    PatchEncoding encoding = it->second;
    if (encoding != IFTB_ENCODING) {
      continue;
    }

    indices.push_back(id);
    FontData new_data;
    new_data.shallow_copy(patch_data);
    data.push_back(std::move(new_data));
  }

  if (!indices.empty()) {
    auto s = ApplyPatches(data, IFTB_ENCODING);
    if (!s.ok()) {
      status_ = s;
      return s;
    }
    for (uint32_t id : indices) {
      pending_patches_.erase(id);
    }
  }

  if (!pending_patches_.empty()) {
    status_ = absl::InternalError(
        "Pending patches remain after processing finished.");
    return status_;
  }

  if (!outstanding_patches_.empty()) {
    return NEEDS_PATCHES;
  }

  return READY;
}

Status IFTClient::ComputeOutstandingPatches() {
  // Patch matching algorithm works like this:
  // 1. Identify all patches listed in the IFT table which intersect the input
  //    codepoints.
  // 2. Keep all of those that are independent.
  // 3. Of the matched dependent patches, keep only one. Select the patch with
  //    the largest coverage.

  if (!ift_table_) {
    outstanding_patches_.clear();
    patch_to_encoding_.clear();
    return absl::OkStatus();
  }

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

  outstanding_patches_.clear();
  patch_to_encoding_.clear();
  for (uint32_t entry_index : independent_entry_indices) {
    const PatchMap::Entry& entry =
        ift_table_->GetPatchMap().GetEntries().at(entry_index);

    auto [it, was_inserted] =
        patch_to_encoding_.insert(std::pair(entry.patch_index, entry.encoding));
    if (!was_inserted && it->second != entry.encoding) {
      return absl::InternalError(
          StrCat("Invalid IFT table. patch ,  ", entry.patch_index,
                 ", has conflicting encoding types: ", entry.encoding,
                 " != ", it->second));
    }

    if (!pending_patches_.contains(entry.patch_index)) {
      outstanding_patches_.insert(entry.patch_index);
    }
  }

  return absl::OkStatus();
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
    case SHARED_BROTLI_ENCODING:
      return brotli_binary_patch_.get();
    case IFTB_ENCODING:
      return iftb_binary_patch_.get();
    case PER_TABLE_SHARED_BROTLI_ENCODING:
      return per_table_binary_patch_.get();
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
  // TODO(garretrieger): consider design space and feature intersections if
  //  codepoint coverage is tied.
  uint32_t selected_entry_index;
  int64_t max_intersection = -1;
  uint32_t min_size = (uint32_t)-1;
  for (uint32_t entry_index : dependent_entry_indices) {
    const PatchMap::Entry& entry =
        ift_table_->GetPatchMap().GetEntries().at(entry_index);

    uint32_t intersection_count =
        intersection_size(&(entry.coverage.codepoints), &target_codepoints_);
    uint32_t size = entry.coverage.codepoints.size();

    if (intersection_count > max_intersection) {
      max_intersection = intersection_count;
      min_size = size;
      selected_entry_index = entry_index;
    } else if (intersection_count == max_intersection && size < min_size) {
      // Break ties in intersection size to the smaller entry.
      min_size = size;
      selected_entry_index = entry_index;
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
