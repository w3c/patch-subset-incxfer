#include "ift/proto/ift_table.h"

#include <google/protobuf/text_format.h>

#include <sstream>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ift/proto/IFT.pb.h"
#include "patch_subset/hb_set_unique_ptr.h"
#include "patch_subset/sparse_bit_set.h"

using absl::flat_hash_map;
using absl::StatusOr;
using patch_subset::FontData;
using patch_subset::hb_set_unique_ptr;
using patch_subset::make_hb_set;
using patch_subset::SparseBitSet;

namespace ift::proto {

constexpr hb_tag_t IFT_TAG = HB_TAG('I', 'F', 'T', ' ');

StatusOr<IFTTable> IFTTable::FromFont(hb_face_t* face) {
  hb_blob_t* ift_table = hb_face_reference_table(face, IFT_TAG);
  if (ift_table == hb_blob_get_empty()) {
    return absl::InvalidArgumentError("'IFT ' table not found in face.");
  }

  unsigned length;
  const char* data = hb_blob_get_data(ift_table, &length);
  std::string data_string(data, length);
  hb_blob_destroy(ift_table);

  IFT ift;
  if (!ift.ParseFromString(data_string)) {
    return absl::InternalError("Unable to parse 'IFT ' table.");
  }

  return FromProto(ift);
}

StatusOr<IFTTable> IFTTable::FromProto(IFT proto) {
  auto m = create_patch_map(proto);
  if (!m.ok()) {
    return m.status();
  }

  return IFTTable(proto, *m);
}

StatusOr<FontData> IFTTable::AddToFont(hb_face_t* face, IFT proto) {
  constexpr uint32_t max_tags = 64;
  hb_tag_t table_tags[max_tags];
  unsigned table_count = max_tags;
  unsigned offset = 0;

  hb_face_t* new_face = hb_face_builder_create();
  while (((void)hb_face_get_table_tags(face, offset, &table_count, table_tags),
          table_count)) {
    for (unsigned i = 0; i < table_count; i++) {
      hb_tag_t tag = table_tags[i];
      hb_blob_t* blob = hb_face_reference_table(face, tag);
      hb_face_builder_add_table(new_face, tag, blob);
      hb_blob_destroy(blob);
    }
    offset += table_count;
  }

  std::string serialized = proto.SerializeAsString();
  hb_blob_t* blob =
      hb_blob_create_or_fail(serialized.data(), serialized.size(),
                             HB_MEMORY_MODE_READONLY, nullptr, nullptr);
  if (!blob) {
    return absl::InternalError(
        "Failed to allocate memory for serialized IFT table.");
  }
  hb_face_builder_add_table(new_face, IFT_TAG, blob);
  hb_blob_destroy(blob);

  blob = hb_face_reference_blob(new_face);
  hb_face_destroy(new_face);
  FontData new_font_data(blob);
  hb_blob_destroy(blob);

  return new_font_data;
}

std::string IFTTable::patch_to_url(uint32_t patch_idx) const {
  std::string url = ift_proto_.url_template();
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
    i = url.find("$", i);
    if (i == std::string::npos) {
      out << url.substr(from);
      break;
    }
    out << url.substr(from, i - from);

    i++;
    if (i == url.length()) {
      out << "$";
      break;
    }

    char c = url[i];
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

const patch_map& IFTTable::get_patch_map() const { return patch_map_; }

StatusOr<patch_map> IFTTable::create_patch_map(const IFT& ift) {
  // TODO(garretrieger): allow for implicit patch indices if they are not
  // specified
  //                     on an entry.
  PatchEncoding default_encoding = ift.default_patch_encoding();
  patch_map result;
  for (auto m : ift.subset_mapping()) {
    uint32_t bias = m.bias();
    uint32_t patch_idx = m.id();
    PatchEncoding encoding = m.patch_encoding();
    if (encoding == DEFAULT_ENCODING) {
      encoding = default_encoding;
    }

    hb_set_unique_ptr codepoints = make_hb_set();
    auto s = SparseBitSet::Decode(m.codepoint_set(), codepoints.get());
    if (!s.ok()) {
      return s;
    }

    hb_codepoint_t cp = HB_SET_VALUE_INVALID;
    while (hb_set_next(codepoints.get(), &cp)) {
      // TODO(garretrieger): currently we assume that a codepoints maps to only
      // one patch,
      //   however, this is not always going to be true. Patch selection needs
      //   to be more complicated then a simple map.
      uint32_t actual_cp = cp + bias;
      if (result.contains(actual_cp)) {
        return absl::InvalidArgumentError(
            "cannot load IFT table that maps a codepoint to more than one "
            "patch.");
      }
      result[actual_cp] = std::pair(patch_idx, encoding);
    }
  }

  return result;
}

}  // namespace ift::proto