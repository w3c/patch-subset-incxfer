#ifndef IFT_IFT_CLIENT_H_
#define IFT_IFT_CLIENT_H_

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "common/binary_patch.h"
#include "common/brotli_binary_patch.h"
#include "common/font_data.h"
#include "hb.h"
#include "ift/iftb_binary_patch.h"
#include "ift/per_table_brotli_binary_patch.h"
#include "ift/proto/IFT.pb.h"
#include "ift/proto/ift_table.h"

namespace ift {

/*
 * Client library for IFT fonts. Provides common operations needed by a client
 * trying to use an IFT font.
 */
class IFTClient {
 public:
  enum State {
    NEEDS_PATCHES,
    READY,
  };

  static absl::StatusOr<IFTClient> NewClient(common::FontData&& font);

 private:
  IFTClient()
      : brotli_binary_patch_(new common::BrotliBinaryPatch()),
        iftb_binary_patch_(new ift::IftbBinaryPatch()),
        per_table_binary_patch_(new ift::PerTableBrotliBinaryPatch()),
        status_(absl::OkStatus()) {}

 public:
  IFTClient(const IFTClient&) = delete;
  IFTClient& operator=(const IFTClient&) = delete;

  IFTClient(IFTClient&& other)
      : font_(std::move(other.font_)),
        face_(other.face_),
        ift_table_(std::move(other.ift_table_)),
        brotli_binary_patch_(std::move(other.brotli_binary_patch_)),
        iftb_binary_patch_(std::move(other.iftb_binary_patch_)),
        per_table_binary_patch_(new ift::PerTableBrotliBinaryPatch()),
        codepoint_to_entries_index_(
            std::move(other.codepoint_to_entries_index_)),
        target_codepoints_(std::move(other.target_codepoints_)),
        outstanding_patches_(std::move(other.outstanding_patches_)),
        pending_patches_(std::move(other.pending_patches_)),
        patch_to_encoding_(std::move(other.patch_to_encoding_)),
        status_(other.status_) {
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
    target_codepoints_ = std::move(other.target_codepoints_);
    outstanding_patches_ = std::move(other.outstanding_patches_);
    pending_patches_ = std::move(other.pending_patches_);
    patch_to_encoding_ = std::move(other.patch_to_encoding_);
    status_ = other.status_;

    return *this;
  }

  ~IFTClient() { hb_face_destroy(face_); }

  static std::string PatchToUrl(const std::string& url_template,
                                uint32_t patch_idx);

  std::string PatchToUrl(uint32_t patch_idx) const {
    return PatchToUrl(ift_table_->GetUrlTemplate(), patch_idx);
  }

  /*
   * Returns the current version of the font. Once Process() returns READY then
   * this font will have been extended to support all codepoints add via
   * AddDesiredCodepoints().
   */
  const common::FontData& GetFontData() { return font_; }

  /*
   * Returns the list of patches that need to be provided to finish processing
   * the current extension request.
   */
  absl::flat_hash_set<uint32_t> PatchesNeeded() const;

  /*
   * Adds a set of codepoints to the target subsets that the font should be
   * extended to cover.
   */
  absl::Status AddDesiredCodepoints(
      const absl::flat_hash_set<uint32_t>& codepoints);

  /*
   * Adds a feature tag to the target subset that the font should be extended
   * to cover.
   */
  absl::Status AddDesiredFeatures(
      const absl::flat_hash_set<hb_tag_t>& features);

  /*
   * Adds an axis range to the target subset taht the font should be extended
   * to cover.
   */
  absl::Status AddDesiredDesignSpace(hb_tag_t axis_tag, float start, float end);

  /*
   * Adds patch data for a patch with the given id.
   */
  void AddPatch(uint32_t id, const common::FontData& font_data);

  /*
   * Call once requested patches have been supplied by AddPatch() in order to
   * apply them. After processing further patches may be needed, which is
   * indicated by returning the NEED_PATCHES state. If all processing is
   * finished and the extended font subset is available returns the FINISHED
   * state.
   */
  absl::StatusOr<State> Process();

 private:
  absl::Status ComputeOutstandingPatches();

  absl::Status ApplyPatches(const std::vector<common::FontData>& patches,
                            ift::proto::PatchEncoding encoding);

  absl::StatusOr<const common::BinaryPatch*> PatcherFor(
      ift::proto::PatchEncoding encoding) const;

  absl::Status SetFont(common::FontData&& new_font);

  void UpdateIndex();

  absl::flat_hash_set<uint32_t> FindCandidateIndices() const;

  void IntersectingEntries(
      const absl::flat_hash_set<uint32_t>& candidate_indices,
      absl::flat_hash_set<uint32_t>& independent_entry_indices,
      absl::btree_set<uint32_t>& dependent_entry_indices);

  uint32_t SelectDependentEntry(
      const absl::btree_set<uint32_t>& dependent_entry_indices);

  common::FontData font_;
  hb_face_t* face_ = nullptr;
  std::optional<ift::proto::IFTTable> ift_table_;

  std::unique_ptr<common::BinaryPatch> brotli_binary_patch_;
  std::unique_ptr<ift::IftbBinaryPatch> iftb_binary_patch_;
  std::unique_ptr<ift::PerTableBrotliBinaryPatch> per_table_binary_patch_;

  static constexpr uint32_t ALL_CODEPOINTS = (uint32_t)-1;
  absl::flat_hash_map<uint32_t, std::vector<uint32_t>>
      codepoint_to_entries_index_;

  absl::flat_hash_set<uint32_t> target_codepoints_;
  absl::flat_hash_set<hb_tag_t> target_features_;
  absl::flat_hash_map<hb_tag_t, ift::proto::PatchMap::AxisRange> design_space_;

  absl::flat_hash_set<uint32_t> outstanding_patches_;
  absl::flat_hash_map<uint32_t, common::FontData> pending_patches_;
  absl::flat_hash_map<uint32_t, ift::proto::PatchEncoding> patch_to_encoding_;
  absl::Status status_;
};

}  // namespace ift

#endif  // IFT_IFT_CLIENT_H_
