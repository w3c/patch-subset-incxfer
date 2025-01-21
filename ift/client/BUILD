cc_library(
  name = "fontations",
  srcs = [
    "fontations_client.cc",
    "fontations_client.h",
  ],
  deps = [
    "//common",
    "//ift/encoder",
    "@abseil-cpp//absl/container:btree",
    "@abseil-cpp//absl/status",
  ],
  data = [
    "@fontations//:ift_graph",
    "@fontations//:ift_extend",
  ],
  visibility = [
    "//ift:__subpackages__",
  ],
)