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
    "//ift/encoder:__pkg__",
  ],
  deps = [
      ":IFT_cc_proto",
      "//common",
      "//patch_subset:common",
      "@com_google_absl//absl/status:statusor",
      "@com_google_absl//absl/container:flat_hash_map",
      "@com_google_absl//absl/container:flat_hash_set",
      "@harfbuzz",
      "@com_google_absl//absl/strings",
  ]
)

cc_test(
    name = "ift_table_test",
    size = "small",
    srcs = [
        "ift_table_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
        "//ift:testdata",
    ],
    deps = [
        ":proto",
        ":IFT_cc_proto",
        "//common",
        "@gtest//:gtest_main",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

proto_library(
  name = "IFT_proto",
  srcs = ["IFT.proto"],
)

cc_proto_library(
  name = "IFT_cc_proto",
  visibility = [
    "//ift:__subpackages__",
    "//util:__pkg__",
    "//patch_subset:__pkg__",
  ],
  deps = [":IFT_proto"],
)

