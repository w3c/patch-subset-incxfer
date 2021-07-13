load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cbor",
    srcs = [
        "cbor_item_unique_ptr.cc",
        "cbor_utils.cc",
        "compressed_int_list.cc",
        "compressed_range_list.cc",
        "compressed_set.cc",
        "int_utils.cc",
    ],
    hdrs = [
        "cbor_item_unique_ptr.h",
        "cbor_utils.h",
        "compressed_int_list.h",
        "compressed_range_list.h",
        "compressed_set.h",
        "int_utils.h",
    ],
    visibility = [
        "//patch_subset:__pkg__",
    ],
    deps = [
        "//common",
        "//patch_subset:constants",
        "@com_google_absl//absl/strings",
        "@libcbor//:cbor",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "cbor_item_unique_ptr_test.cc",
        "cbor_utils_test.cc",
        "compressed_int_list_test.cc",
        "compressed_range_list_test.cc",
        "compressed_set_test.cc",
        "int_utils_test.cc",
    ],
    deps = [
        ":cbor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
        "@libcbor//:cbor",
    ],
)

