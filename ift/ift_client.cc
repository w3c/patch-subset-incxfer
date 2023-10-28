#include "ift/ift_client.h"

#include <sstream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "ift/proto/patch_map.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

using absl::flat_hash_map;
using absl::flat_hash_set;
using absl::Status;
using absl::StatusOr;
using absl::StrCat;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::PatchMap;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BinaryPatch;
using patch_subset::FontData;

namespace ift {

StatusOr<IFTClient> IFTClient::NewClient(patch_subset::FontData&& font) {
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

StatusOr<patch_set> IFTClient::PatchUrlsFor(
    const hb_set_t& additional_codepoints) const {
  // Patch matching algorithm works like this:
  // 1. Identify all patches listed in the IFT table which intersect the input
  // codepoints.
  // 2. Keep all of those that are independent.
  // 3. Of the matched dependent patches, keep only one. Select the patch with
  // the largest
  //    coverage.

  if (!ift_table_) {
    patch_set result;
    return result;
  }

  absl::flat_hash_set<uint32_t> independent_entry_indices;
  absl::flat_hash_set<uint32_t> dependent_entry_indices;

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(&additional_codepoints, &cp)) {
    auto indices = codepoint_to_entries_index_.find(cp);
    if (indices == codepoint_to_entries_index_.end()) {
      continue;
    }

    for (uint32_t index : indices->second) {
      const auto& entry = ift_table_->GetPatchMap().GetEntries().at(index);

      if (entry.IsDependent()) {
        dependent_entry_indices.insert(index);
      } else {
        independent_entry_indices.insert(index);
      }
    }

    if (!dependent_entry_indices.empty()) {
      // Pick at most one dependent patches to keep.
      // TODO(garretrieger): use intersection size with additional_codepoints
      // instead.
      // TODO(garretrieger): merge coverages when multiple entries have the same
      // patch index.

      uint32_t selected_entry_index;
      uint32_t max_size = 0;
      for (uint32_t entry_index : dependent_entry_indices) {
        const PatchMap::Entry& entry =
            ift_table_->GetPatchMap().GetEntries().at(entry_index);
        if (entry.coverage.codepoints.size() > max_size) {
          max_size = entry.coverage.codepoints.size();
          selected_entry_index = entry_index;
        }
      }
      independent_entry_indices.insert(selected_entry_index);
    }
  }

  patch_set result;
  for (uint32_t entry_index : independent_entry_indices) {
    const PatchMap::Entry& entry =
        ift_table_->GetPatchMap().GetEntries().at(entry_index);
    std::string url =
        IFTClient::PatchToUrl(ift_table_->GetUrlTemplate(), entry.patch_index);

    auto [it, was_inserted] =
        result.insert(std::pair(std::move(url), entry.encoding));
    if (!was_inserted && it->second != entry.encoding) {
      return absl::InternalError(StrCat("Invalid IFT table. patch URL,  ", url,
                                        ", has conflicting encoding types: ",
                                        entry.encoding, " != ", it->second));
    }
  }

  return result;
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
    default:
      std::stringstream message;
      message << "Patch encoding " << encoding << " is not implemented.";
      return absl::UnimplementedError(message.str());
  }
}

Status IFTClient::SetFont(patch_subset::FontData&& new_font) {
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

void IFTClient::UpdateIndex() {
  codepoint_to_entries_index_.clear();
  if (!ift_table_) {
    return;
  }

  uint32_t entry_index = 0;
  for (const auto& e : ift_table_->GetPatchMap().GetEntries()) {
    for (uint32_t cp : e.coverage.codepoints) {
      codepoint_to_entries_index_[cp].push_back(entry_index);
    }
    entry_index++;
  }
}

}  // namespace ift