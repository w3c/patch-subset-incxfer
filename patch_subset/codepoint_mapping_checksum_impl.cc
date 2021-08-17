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

    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    return *this;
  }

 private:
  uint8_t data[8];
};

uint64_t CodepointMappingChecksumImpl::Checksum(
    const CodepointRemappingProto& response) const {
  // See: https://w3c.github.io/IFT/Overview.html#computing-checksums
  // for details of checksum algorithm.
  int num_deltas = response.codepoint_ordering().deltas_size();
  std::vector<LittleEndianInt> data(num_deltas);

  int32_t previous = 0;
  for (int i = 0; i < num_deltas; i++) {
    int32_t next = previous + response.codepoint_ordering().deltas(i);
    data[i] = next;
    previous = next;
  }

  return this->hasher_->Checksum(
      absl::string_view(reinterpret_cast<char*>(data.data()),
                        data.size() * sizeof(LittleEndianInt)));
}

}  // namespace patch_subset
