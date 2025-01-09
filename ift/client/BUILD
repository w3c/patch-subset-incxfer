cc_library(
  name = "fontations",
  srcs = [
    "fontations_client.cc",
    "fontations_client.h",
  ],
  deps = [
    "//common",
    "//ift/encoder",
    "@com_google_absl//absl/container:btree",
    "@com_google_absl//absl/status",
  ],
  data = [
    "@fontations//:ift_graph",
    "@fontations//:ift_extend",
  ],
  visibility = [
    "//ift:__subpackages__",
  ],
)