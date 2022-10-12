#ifndef BROTLI_HMTX_DIFFER_H_
#define BROTLI_HMTX_DIFFER_H_

#include "absl/types/span.h"
#include "brotli/table_differ.h"

namespace brotli {

class HmtxDiffer : public TableDiffer {
 private:
  enum Mode {
    INIT = 0,
    NEW_DATA,
    EXISTING_DATA,
  } mode = INIT;

  unsigned base_number_of_metrics;
  unsigned derived_number_of_metrics;

 public:
  HmtxDiffer(absl::Span<const uint8_t> base_hhea,
             absl::Span<const uint8_t> derived_hhea)
      : base_number_of_metrics(GetNumberOfMetrics(base_hhea)),
        derived_number_of_metrics(GetNumberOfMetrics(derived_hhea)) {}

  unsigned Process(unsigned derived_gid, unsigned base_gid,
                   unsigned base_derived_gid) override {
    mode = NEW_DATA;
    if (derived_gid == base_derived_gid) {
      if ((derived_gid < derived_number_of_metrics &&
           base_gid < base_number_of_metrics) ||
          (derived_gid >= derived_number_of_metrics &&
           base_gid >= base_number_of_metrics)) {
        // Can only copy existing data if both base and derived are on the same
        // side of number of metrics.
        mode = EXISTING_DATA;
      }
    }

    return derived_gid < derived_number_of_metrics ? 4 : 2;
  }

  unsigned Finalize() const override {
    // noop
    return 0;
  }

  bool IsNewData() const override { return mode == NEW_DATA; }

 private:
  static unsigned GetNumberOfMetrics(absl::Span<const uint8_t> hhea) {
    constexpr unsigned field_offset = 34;

    if (hhea.size() < field_offset + 2) {
      return 0;
    }

    const uint8_t* num_metrics = hhea.data() + field_offset;
    return (num_metrics[0] << 8) | num_metrics[1];
  }
};

}  // namespace brotli

#endif  // BROTLI_HMTX_DIFFER_H_
