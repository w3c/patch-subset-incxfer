load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")

cc_library(
  name = "proto",
  srcs = [
    "ift_table.h",
    "ift_table.cc",
    "patch_map.h",
    "patch_map.cc",
    "format_2_patch_map.cc",
    "format_2_patch_map.h",
    "patch_encoding.h",
  ],
  visibility = [
    "//util:__pkg__",
    "//ift:__pkg__",
    "//ift/encoder:__pkg__",
  ],
  deps = [
      "//ift/feature_registry",
      "//common",
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
        "//common:testdata",
        "//ift:testdata",
    ],
    deps = [
        ":proto",
        "//common",
        "@gtest//:gtest_main",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_test(
    name = "patch_map_test",
    size = "small",
    srcs = [
        "patch_map_test.cc",
        "format_2_patch_map_test.cc",
    ],
    data = [
        "//common:testdata",
        "//ift:testdata",
    ],
    deps = [
        ":proto",
        "//common",
        "@gtest//:gtest_main",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)