load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cbor",
    srcs = [
        "cbor_unique_ptr.cc",
        "int_utils.cc",
    ],
    hdrs = [
        "cbor_unique_ptr.h",
        "int_utils.h",
    ],
    visibility = [
        "//patch_subset:__pkg__",
    ],
    deps = [
        "//common",
        "@com_google_absl//absl/strings",
        "@libcbor//:AJcbor",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "cbor_unique_ptr_tests.cc",
        "int_utils_test.cc",
    ],
    deps = [
        ":cbor",
        "@com_google_absl//absl/strings",
        "@gtest//:gtest_main",
        "@libcbor//:cbor",
    ],
)

