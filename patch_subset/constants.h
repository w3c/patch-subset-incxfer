#ifndef PATCH_SUBSET_CONSTANTS_H_
#define PATCH_SUBSET_CONSTANTS_H_

namespace patch_subset {

enum ProtocolVersion {
  ONE = 0,
};

/**
 * TODO: more docs
 * https://w3c.github.io/PFE/Overview.html#patch-formats
 */
enum PatchFormat {
  VCDIFF = 0,
  BROTLI_SHARED_DICT = 1,
};

enum ConnectionSpeed {
  VERY_SLOW = 1,       //  > 1000 ms.
  SLOW = 2,            // [300 ms, 1000 ms)
  AVERAGE = 3,         //  [150 ms, 300 ms)
  FAST = 4,            // [80 ms, 150 ms)
  VERY_FAST = 5,       // [20 ms, 80 ms)
  EXTREMELY_FAST = 6,  // [0 ms, 20 ms)
};
}  // namespace patch_subset

#endif  // PATCH_SUBSET_CONSTANTS_H_
