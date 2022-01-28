#include "patch_subset/integer_list_checksum_impl.h"

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

uint64_t IntegerListChecksumImpl::Checksum(
    const std::vector<int32_t>& ints) const {
  // See: https://w3c.github.io/IFT/Overview.html#computing-checksums
  // for details of checksum algorithm.
  std::vector<LittleEndianInt> data(ints.size());
  for (uint32_t i = 0; i < ints.size(); i++) {
    data[i] = ints[i];  // Converts to little endian.
  }
  return this->hasher_->Checksum(
      absl::string_view(reinterpret_cast<char*>(data.data()),
                        data.size() * sizeof(LittleEndianInt)));
}

}  // namespace patch_subset
