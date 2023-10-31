#ifndef IFT_PROTO_PATCH_MAP_H_
#define IFT_PROTO_PATCH_MAP_H_

#include <cstdint>
#include <initializer_list>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ift/proto/IFT.pb.h"

namespace ift::proto {

/*
 * Abstract representation of a map from subset definitions too patches.
 */
class PatchMap {
 public:
  struct Coverage {
    // TODO(garretrieger): move constructors?
    static absl::StatusOr<Coverage> FromProto(
        const ift::proto::SubsetMapping& mapping);

    Coverage() {}
    Coverage(std::initializer_list<uint32_t> codepoints_list)
        : codepoints(codepoints_list) {}

    friend void PrintTo(const Coverage& point, std::ostream* os);

    bool operator==(const Coverage& other) const {
      return other.codepoints == codepoints && other.features == features;
    }

    void ToProto(ift::proto::SubsetMapping* out) const;

    // TODO(garretrieger): use hb sets instead?
    absl::flat_hash_set<uint32_t> codepoints;
    absl::flat_hash_set<uint32_t> features;
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
             other.encoding == encoding;
    }

    void ToProto(uint32_t last_patch_index,
                 ift::proto::PatchEncoding default_encoding,
                 ift::proto::SubsetMapping* out) const;

    bool IsDependent() const {
      return encoding == SHARED_BROTLI_ENCODING ||
             encoding == PER_TABLE_SHARED_BROTLI_ENCODING;
    }

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
                ift::proto::PatchEncoding);
  void RemoveEntries(uint32_t patch_index);

 private:
  // TODO(garretrieger): keep an index which maps from patch_index to entry
  // index for faster deletions.
  std::vector<Entry> entries_;
};

}  // namespace ift::proto

#endif  // IFT_PROTO_PATCH_MAP_H_