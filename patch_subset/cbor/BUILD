load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cbor",
    srcs = [
        "cbor_item_unique_ptr.cc",
        "cbor_utils.cc",
        "client_state.cc",
        "compressed_set.cc",
        "int_utils.cc",
        "integer_list.cc",
        "patch_format_fields.cc",
        "patch_request.cc",
        "patch_response.cc",
        "range_list.cc",
    ],
    hdrs = [
        "cbor_item_unique_ptr.h",
        "cbor_utils.h",
        "client_state.h",
        "compressed_set.h",
        "int_utils.h",
        "integer_list.h",
        "patch_format_fields.h",
        "patch_request.h",
        "patch_response.h",
        "range_list.h",
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
        "client_state_test.cc",
        "compressed_set_test.cc",
        "int_utils_test.cc",
        "integer_list_test.cc",
        "patch_format_fields_test.cc",
        "patch_request_test.cc",
        "patch_response_test.cc",
        "range_list_test.cc",
    ],
    deps = [
        ":cbor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
        "@libcbor//:cbor",
    ],
)
