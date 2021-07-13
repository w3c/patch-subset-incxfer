#ifndef PATCH_SUBSET_CBOR_INT_UTILS_H_
#define PATCH_SUBSET_CBOR_INT_UTILS_H_

#include "absl/strings/string_view.h"
#include "common/status.h"

namespace patch_subset::cbor {

class IntUtils {
 public:
  // TODO: doc
  static uint32_t ZigZagEncode(int32_t signed_int);
  static int32_t ZigZagDecode(uint32_t unsigned_int);

  // Encode an unsigned 32 bit integer as 1..5 bytes, depending on its
  // magnitude. Buffer is the location to start writing bytes to, with at most
  // size_in_out bytes allowed to be written. size_in_out is set to the number
  // of bytes which were required to encode the integer. In the event that
  // writing the integer would exceed size_in_out bytes, the buffer is not
  // written to - use the value set in size_in_out to expand the buffer.
  static StatusCode UIntBase128Encode(uint32_t unsigned_int, uint8_t* buffer,
                                      size_t* size_in_out);

  // Reads 1..5 bytes and decodes to an unsigned 32 bit int. num_bytes_out is
  // set to the number of bytes read, or zero if the data is invalid, or the end
  // of the buffer was reached while decoding.
  static StatusCode UintBase128Decode(absl::string_view bytes, uint32_t* uint_out,
                                      size_t* num_bytes_out);

  //  The number of bytes required to encode the unsigned 32 bit value.
  static int UintBase128EncodedSize(uint32_t uint);
};

}  // namespace patch_subset::cbor

#endif  // PATCH_SUBSET_CBOR_INT_UTILS_H_
