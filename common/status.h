#ifndef COMMON_STATUS_H_
#define COMMON_STATUS_H_

#include <iostream>

namespace patch_subset {

enum class StatusCode : int {
  kOk = 0,
  kInvalidArgument = 1,
  kNotFound = 2,
  kFailedPrecondition = 3,
  kUnimplemented = 4,
  kInternal = 5,
};

inline std::ostream& operator<<(std::ostream& os, StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      os << "OK";
      break;
    case StatusCode::kInvalidArgument:
      os << "INVALID ARGUMENT";
      break;
    case StatusCode::kNotFound:
      os << "NOT FOUND";
      break;
    case StatusCode::kFailedPrecondition:
      os << "FAILED PRECONDITION";
      break;
    case StatusCode::kUnimplemented:
      os << "UNIMPLEMENTED";
      break;
    case StatusCode::kInternal:
      os << "INTERNAL";
      break;
  }
  return os;
}

}  // namespace patch_subset

#endif  // COMMON_STATUS_H_
