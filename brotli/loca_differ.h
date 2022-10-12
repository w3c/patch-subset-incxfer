#ifndef BROTLI_LOCA_DIFFER_H_
#define BROTLI_LOCA_DIFFER_H_

#include "absl/types/span.h"
#include "brotli/table_differ.h"

namespace brotli {

class LocaDiffer : public TableDiffer {
 private:
  enum Mode {
    INIT = 0,
    NEW_DATA,
    EXISTING_DATA,
  } mode = INIT;

  unsigned loca_width_;

 public:
  LocaDiffer(bool is_short_loca) : loca_width_(is_short_loca ? 2 : 4) {}

  unsigned Process(unsigned derived_gid, unsigned base_gid,
                   unsigned base_derived_gid) override {
    switch (mode) {
      case INIT:
      case EXISTING_DATA:
        mode = (base_derived_gid == derived_gid) ? EXISTING_DATA : NEW_DATA;
        break;
      case NEW_DATA:
        // Once new data is encountered all remaining data must be new since
        // loca entries depend on previous loca entries.
        break;
    }
    return loca_width_;
  }

  unsigned Finalize() override {
    // Loca table has one extra entry at the end. Stay in current mode.
    return loca_width_;
  }

  bool IsNewData() const override { return mode == NEW_DATA; }

 private:
};

}  // namespace brotli

#endif  // BROTLI_LOCA_DIFFER_H_
