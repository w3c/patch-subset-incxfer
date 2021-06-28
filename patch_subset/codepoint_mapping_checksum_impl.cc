#include "patch_subset/codepoint_mapping_checksum_impl.h"

#include <vector>

#include "absl/strings/string_view.h"

namespace patch_subset {

struct LittleEndianInt {
 public:
  LittleEndianInt& operator=(int32_t value) {
    data[0] = (value)&0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
    return *this;
  }

 private:
  uint8_t data[4];
};

uint64_t CodepointMappingChecksumImpl::Checksum(
    const CodepointRemappingProto& response) const {
  int num_deltas = response.codepoint_ordering().deltas_size();

  std::vector<LittleEndianInt> data(num_deltas + 1);
  data[0] = num_deltas;
  for (int i = 0; i < num_deltas; i++) {
    data[i + 1] = response.codepoint_ordering().deltas(i);
  }

  // TODO(garretrieger): implement checksumming of the grouping strategy.

  return this->hasher_->Checksum(
      absl::string_view(reinterpret_cast<char*>(data.data()),
                        data.size() * sizeof(LittleEndianInt)));
}

}  // namespace patch_subset
