#ifndef IFT_PROTO_PATCH_MAP_H_
#define IFT_PROTO_PATCH_MAP_H_

#include <cstdint>
#include <initializer_list>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ift/proto/IFT.pb.h"

namespace ift::proto {

/*
 * Abstract representation of a map from subset definitions too patches.
 */
class PatchMap {
  // TODO figure out what this interface should look like.
  // - it should allow a patch selection implementation to use a gid mapped
  // table
  //   using direct access to the table's memory.
  // - but also work with a implementeation where the map has been loaded into
  // data
  //   structures in memory.
  //
  // Patch selection algorithm takes a codepoint set and set of features and
  // then returns a set of patch indices.
  //
  // Probably want a prefilter that selects candidate patches based on codepoint
  // only then the selection algorithm can grab the details of each patches
  // coverage in order to compare and make final selections.
  //
  // Keep in mind the differing selection algorithms for dependent vs
  // independent.
  //
  // Maybe we should actually not expose this, and only have a method to produce
  // the selections. This would have a IFTB gid map specific algorithm and then
  // a more generic implementation which uses a in memory abstract version of
  // the table.
  //
  // The in memory abstract map could then be deserialized/serialized back to
  // either subtable type. For modification operations.
  //
  // So then we have:
  // 1. Abstract patch selection algorithm:
  //    a. Impl 1: using in memory patch map.
  //    b. Impl 2: using IFTB gid/feature map directly.
  // 2. In Memory Patch map, with support for modification. (THIS FILE)
  // 3. Serialize/deserialize implementations from memory patch map to/from the
  // protobuf
  //    and IFTB mapping formats.
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
          PatchEncoding enc)
        : coverage(codepoints), patch_index(patch_idx), encoding(enc) {}

    friend void PrintTo(const Entry& point, std::ostream* os);

    bool operator==(const Entry& other) const {
      return other.coverage == coverage && other.patch_index == patch_index &&
             other.encoding == encoding;
    }

    Coverage coverage;
    uint32_t patch_index;
    PatchEncoding encoding;
  };

  // TODO(garretrieger): move constructors?
  PatchMap() {}
  PatchMap(std::initializer_list<Entry> entries) : entries_(entries) {}

  friend void PrintTo(const PatchMap& point, std::ostream* os);

  bool operator==(const PatchMap& other) const {
    return other.entries_ == entries_;
  }

  static absl::StatusOr<PatchMap> FromProto(const ift::proto::IFT& ift_proto);

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