#ifndef IFT_PROTO_PATCH_MAP_H_
#define IFT_PROTO_PATCH_MAP_H_

#include <cstdint>
#include <initializer_list>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/axis_range.h"
#include "hb.h"
#include "ift/proto/IFT.pb.h"

namespace ift::proto {

/*
 * Abstract representation of a map from subset definitions too patches.
 */
class PatchMap {
 public:
  static bool IsDependent(PatchEncoding encoding) {
    return encoding == SHARED_BROTLI_ENCODING ||
           encoding == PER_TABLE_SHARED_BROTLI_ENCODING;
  }

  enum Modification {
    MODIFIED_NEITHER,
    MODIFIED_MAIN,
    MODIFIED_EXTENSION,
    MODIFIED_BOTH,
  };

  static absl::StatusOr<common::AxisRange> FromProto(
      const ift::proto::AxisRange& range) {
    return common::AxisRange::Range(range.start(), range.end());
  }

  static void ToProto(const common::AxisRange& range,
                      ift::proto::AxisRange* out) {
    out->set_start(range.start());
    out->set_end(range.end());
  }

  struct Coverage {
    // TODO(garretrieger): move constructors?
    static absl::StatusOr<Coverage> FromProto(
        const ift::proto::SubsetMapping& mapping);

    Coverage() {}
    Coverage(std::initializer_list<uint32_t> codepoints_list)
        : codepoints(codepoints_list) {}
    Coverage(const absl::flat_hash_set<uint32_t>& codepoints_list)
        : codepoints(codepoints_list) {}

    friend void PrintTo(const Coverage& point, std::ostream* os);

    bool operator==(const Coverage& other) const {
      return other.codepoints == codepoints && other.features == features &&
             other.design_space == design_space;
    }

    void ToProto(ift::proto::SubsetMapping* out) const;

    bool Intersects(const absl::flat_hash_set<uint32_t>& codepoints,
                    const absl::flat_hash_set<hb_tag_t>& features,
                    const absl::flat_hash_map<hb_tag_t, common::AxisRange>&
                        design_space) const;

    uint32_t SmallestCodepoint() const {
      uint32_t min = 0xFFFFFFFF;
      for (uint32_t cp : codepoints) {
        if (cp < min) {
          min = cp;
        }
      }
      return min;
    }

    // TODO(garretrieger): use hb sets instead?
    absl::flat_hash_set<uint32_t> codepoints;
    absl::flat_hash_set<hb_tag_t> features;
    absl::btree_map<hb_tag_t, common::AxisRange> design_space;
  };

  struct Entry {
    // TODO(garretrieger): move constructors?
    static absl::StatusOr<Entry> FromProto(
        const ift::proto::SubsetMapping& mapping, uint32_t index,
        ift::proto::PatchEncoding encoding);

    Entry() {}
    Entry(std::initializer_list<uint32_t> codepoints, uint32_t patch_idx,
          PatchEncoding enc, bool is_ext = false)
        : coverage(codepoints),
          patch_index(patch_idx),
          encoding(enc),
          extension_entry(is_ext) {}

    friend void PrintTo(const Entry& point, std::ostream* os);

    bool operator==(const Entry& other) const {
      return other.coverage == coverage && other.patch_index == patch_index &&
             other.encoding == encoding &&
             other.extension_entry == extension_entry;
    }

    void ToProto(uint32_t last_patch_index,
                 ift::proto::PatchEncoding default_encoding,
                 ift::proto::SubsetMapping* out) const;

    bool IsDependent() const { return PatchMap::IsDependent(encoding); }

    Coverage coverage;
    uint32_t patch_index;
    PatchEncoding encoding;
    bool extension_entry = false;
  };

  // TODO(garretrieger): move constructors?
  PatchMap() {}
  PatchMap(std::initializer_list<Entry> entries) : entries_(entries) {}

  friend void PrintTo(const PatchMap& point, std::ostream* os);

  bool operator==(const PatchMap& other) const {
    return other.entries_ == entries_;
  }

  static absl::StatusOr<PatchMap> FromProto(const ift::proto::IFT& ift_proto);
  absl::Status AddFromProto(const ift::proto::IFT& ift_proto,
                            bool is_extension_table = false);

  void AddToProto(ift::proto::IFT& ift_proto,
                  bool extension_entries = false) const;

  absl::Span<const Entry> GetEntries() const;

  void AddEntry(const Coverage& coverage, uint32_t patch_index,
                ift::proto::PatchEncoding, bool is_extension = false);

  Modification RemoveEntries(uint32_t patch_index);

 private:
  // TODO(garretrieger): keep an index which maps from patch_index to entry
  // index for faster deletions.
  std::vector<Entry> entries_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_PATCH_MAP_H_