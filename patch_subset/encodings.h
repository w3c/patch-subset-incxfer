#ifndef PATCH_SUBSET_ENCODINGS_H_
#define PATCH_SUBSET_ENCODINGS_H_

namespace patch_subset {

namespace Encodings {

static const char* kIdentityEncoding [[maybe_unused]] = "identity";
static const char* kBrotliDiffEncoding [[maybe_unused]] = "brdiff";
static const char* kVCDIFFEncoding [[maybe_unused]] = "vcdiff";

}  // namespace Encodings

}  // namespace patch_subset

#endif  // PATCH_SUBSET_ENCODINGS_H_
