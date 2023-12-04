#include "common/axis_range.h"

namespace common {

void PrintTo(const AxisRange& range, std::ostream* os) {
  *os << "[" << range.start() << ", " << range.end() << "]";
}

}  // namespace common