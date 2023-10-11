#include "ift/ift_client.h"

#include <sstream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/font_data.h"

using absl::Status;
using absl::StatusOr;
using ift::proto::IFTB_ENCODING;
using ift::proto::IFTTable;
using ift::proto::PatchEncoding;
using ift::proto::SHARED_BROTLI_ENCODING;
using patch_subset::BinaryPatch;
using patch_subset::FontData;

namespace ift {

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
    const FontData& font, const hb_set_t& additional_codepoints) const {
  hb_face_t* face = font.reference_face();
  auto ift = IFTTable::FromFont(face);
  hb_face_destroy(face);

  patch_set result;
  if (absl::IsNotFound(ift.status())) {
    // No IFT table, means there's no additional patches.
    return result;
  }

  if (!ift.ok()) {
    return ift.status();
  }

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  while (hb_set_next(&additional_codepoints, &cp)) {
    auto v = ift->GetPatchMap().find(cp);
    if (v == ift->GetPatchMap().end()) {
      continue;
    }

    uint32_t patch_idx = v->second.first;
    PatchEncoding encoding = v->second.second;
    result.insert(std::pair(
        IFTClient::PatchToUrl(ift->GetUrlTemplate(), patch_idx), encoding));
  }

  return result;
}

StatusOr<FontData> IFTClient::ApplyPatches(const FontData& font,
                                           const std::vector<FontData>& patches,
                                           PatchEncoding encoding) const {
  auto patcher = PatcherFor(encoding);
  if (!patcher.ok()) {
    return patcher.status();
  }

  FontData result;
  Status s = (*patcher)->Patch(font, patches, &result);
  if (!s.ok()) {
    return s;
  }

  return result;
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

}  // namespace ift