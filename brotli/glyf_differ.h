#ifndef BROTLI_GLYF_DIFFER_H_
#define BROTLI_GLYF_DIFFER_H_

#include "absl/types/span.h"
#include "brotli/table_differ.h"

namespace brotli {

class GlyfDiffer : public TableDiffer {
 private:
  enum Mode {
    INIT = 0,
    NEW_DATA,
    EXISTING_DATA,
  } mode = INIT;

  absl::Span<const uint8_t> loca_;
  bool use_short_loca_;

 public:
  GlyfDiffer(absl::Span<const uint8_t> loca, bool use_short_loca)
      : loca_(loca), use_short_loca_(use_short_loca) {}

  unsigned Process(unsigned derived_gid, unsigned base_gid,
                   unsigned base_derived_gid) override {
    mode = (base_derived_gid == derived_gid) ? EXISTING_DATA : NEW_DATA;
    return GlyphLength(derived_gid);
  }

  unsigned Finalize() const override {
    // noop
    return 0;
  }

  bool IsNewData() const override { return mode == NEW_DATA; }

 private:
  // Length of glyph (in bytes) found in the derived subset.
  unsigned GlyphLength(unsigned gid) {
    const uint8_t* derived_loca = loca_.data();

    if (use_short_loca_) {
      unsigned index = gid * 2;

      unsigned off_1 = ((derived_loca[index] & 0xFF) << 8) |
                       (derived_loca[index + 1] & 0xFF);

      index += 2;
      unsigned off_2 = ((derived_loca[index] & 0xFF) << 8) |
                       (derived_loca[index + 1] & 0xFF);

      return (off_2 * 2) - (off_1 * 2);
    }

    unsigned index = gid * 4;

    unsigned off_1 = ((derived_loca[index] & 0xFF) << 24) |
                     ((derived_loca[index + 1] & 0xFF) << 16) |
                     ((derived_loca[index + 2] & 0xFF) << 8) |
                     (derived_loca[index + 3] & 0xFF);

    index += 4;
    unsigned off_2 = ((derived_loca[index] & 0xFF) << 24) |
                     ((derived_loca[index + 1] & 0xFF) << 16) |
                     ((derived_loca[index + 2] & 0xFF) << 8) |
                     (derived_loca[index + 3] & 0xFF);

    return off_2 - off_1;
  }
};

}  // namespace brotli

#endif  // BROTLI_GLYF_DIFFER_H_
