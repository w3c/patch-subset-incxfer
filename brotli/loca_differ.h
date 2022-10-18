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

  bool mismatched_loca_format_;
  unsigned loca_width_;

 public:
  // TODO(garretrieger): we need to check if both base and derived are short
  // loca. if they don't match than all entries are new.
  LocaDiffer(bool is_base_short_loca,
             bool is_derived_short_loca)
      : mismatched_loca_format_(is_base_short_loca != is_derived_short_loca),
        loca_width_(is_derived_short_loca ? 2 : 4) {}

  void Process(unsigned derived_gid, unsigned base_gid,
               unsigned base_derived_gid, bool is_base_empty,
               unsigned* base_delta, /* OUT */
               unsigned* derived_delta /* OUT */) override {
    *derived_delta = loca_width_;
    if (mismatched_loca_format_) {
      // If loca format is not the same between base and derived then we can't re-use
      // any data from the base loca.
      mode = NEW_DATA;
    }

    switch (mode) {
      case INIT:
      case EXISTING_DATA:
        if (base_derived_gid == derived_gid) {
          mode = EXISTING_DATA;
          *base_delta = loca_width_;
          break;
        }

        mode = NEW_DATA;
        // fallthrough
      case NEW_DATA:

        // Once new data is encountered all remaining data must be new since
        // loca entries depend on previous loca entries.
        *base_delta = 0;
        break;
    }
  }

  void Finalize(unsigned* base_delta, /* OUT */
                unsigned* derived_delta /* OUT */) const override {
    // Loca table has one extra entry at the end. Stay in current mode.
    *base_delta = loca_width_;
    *derived_delta = loca_width_;
  }

  bool IsNewData() const override { return mode == NEW_DATA; }

 private:
};

}  // namespace brotli

#endif  // BROTLI_LOCA_DIFFER_H_
