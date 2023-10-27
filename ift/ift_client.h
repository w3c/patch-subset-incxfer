#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include "absl/container/btree_map.h"
#include "absl/status/statusor.h"
#include "hb.h"
#include "ift/iftb_binary_patch.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"
#include "patch_subset/binary_patch.h"
#include "patch_subset/brotli_binary_patch.h"
#include "patch_subset/font_data.h"

namespace ift {

typedef absl::btree_map<std::string, ift::proto::PatchEncoding> patch_set;

/*
 * Client library for IFT fonts. Provides common operations needed by a client
 * trying to use an IFT font.
 */
class IFTClient {
 public:
  static absl::StatusOr<IFTClient> NewClient(patch_subset::FontData&& font);

 private:
  IFTClient()
      : brotli_binary_patch_(new patch_subset::BrotliBinaryPatch()),
        iftb_binary_patch_(new ift::IftbBinaryPatch()) {}

 public:
  IFTClient(const IFTClient&) = delete;
  IFTClient& operator=(const IFTClient&) = delete;

  IFTClient(IFTClient&& other)
      : font_(std::move(other.font_)),
        face_(other.face_),
        ift_table_(std::move(other.ift_table_)),
        brotli_binary_patch_(std::move(other.brotli_binary_patch_)),
        iftb_binary_patch_(std::move(other.iftb_binary_patch_)),
        codepoint_to_entries_index_(
            std::move(other.codepoint_to_entries_index_)) {
    other.face_ = nullptr;
  }

  IFTClient& operator=(IFTClient&& other) {
    if (this == &other) {
      return *this;
    }

    font_ = std::move(other.font_);
    face_ = other.face_;
    other.face_ = nullptr;
    ift_table_ = std::move(other.ift_table_);
    brotli_binary_patch_ = std::move(other.brotli_binary_patch_);
    iftb_binary_patch_ = std::move(other.iftb_binary_patch_);
    codepoint_to_entries_index_ = std::move(other.codepoint_to_entries_index_);

    return *this;
  }

  ~IFTClient() { hb_face_destroy(face_); }

  static std::string PatchToUrl(const std::string& url_template,
                                uint32_t patch_idx);

  const patch_subset::FontData& GetFontData() { return font_; }

  /*
   * Returns the set of patches needed to add support for all codepoints
   * in 'additional_codepoints'.
   */
  absl::StatusOr<patch_set> PatchUrlsFor(
      const hb_set_t& additional_codepoints) const;

  /*
   * Applies one or more 'patches' to 'font'. The patches are all encoded
   * in the 'encoding' format.
   *
   * Returns the extended font.
   */
  absl::Status ApplyPatches(const std::vector<patch_subset::FontData>& patches,
                            ift::proto::PatchEncoding encoding);

 private:
  absl::StatusOr<const patch_subset::BinaryPatch*> PatcherFor(
      ift::proto::PatchEncoding encoding) const;

  absl::Status SetFont(patch_subset::FontData&& new_font);

  void UpdateIndex();

  patch_subset::FontData font_;
  hb_face_t* face_ = nullptr;
  std::optional<ift::proto::IFTTable> ift_table_;

  std::unique_ptr<patch_subset::BinaryPatch> brotli_binary_patch_;
  std::unique_ptr<ift::IftbBinaryPatch> iftb_binary_patch_;

  absl::flat_hash_map<uint32_t, std::vector<uint32_t>>
      codepoint_to_entries_index_;
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_