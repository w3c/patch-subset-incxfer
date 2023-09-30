load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")

cc_library(
  name = "proto",
  srcs = [
    "ift_table.h",
    "ift_table.cc",
  ],
  visibility = [
    "//util:__pkg__",
    "//ift:__pkg__",
  ],
  deps = [
      ":IFT_cc_proto",
      "//patch_subset:common",
      "@com_google_absl//absl/status:statusor",
      "@com_google_absl//absl/container:flat_hash_map",
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
    "//ift:__pkg__",
    "//util:__pkg__",
    "//patch_subset:__pkg__",
  ],
  deps = [":IFT_proto"],
)

