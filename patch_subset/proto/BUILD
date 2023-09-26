load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")

cc_library(
  name = "proto",
  srcs = [
    "ift_table.h",
    "ift_table.cc",
  ],
  deps = [
      ":IFT_cc_proto",
      "@com_google_absl//absl/status:statusor",
      "@harfbuzz",
      "@com_google_absl//absl/strings",
  ]
)

proto_library(
  name = "IFT_proto",
  srcs = ["IFT.proto"],
)

cc_proto_library(
  name = "IFT_cc_proto",
  visibility = [
    "//patch_subset:__pkg__",
  ],
  deps = [":IFT_proto"],
)

