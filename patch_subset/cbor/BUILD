load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cbor",
    srcs = [
        "int_utils.cc",
    ],
    hdrs = [
        "int_utils.h",
    ],
    visibility = [
        "//patch_subset:__pkg__",
    ],
    deps = [
        "//common",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "int_utils_test.cc",
    ],
    deps = [
        ":cbor",
        "@com_google_absl//absl/strings",
        "@gtest//:gtest_main",
    ],
)

