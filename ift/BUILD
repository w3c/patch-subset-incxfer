cc_library(
    name = "client",
    srcs = [
        "ift_client.cc",
        "iftb_binary_patch.cc",
        "iftb_binary_patch.h",
        "per_table_brotli_binary_diff.cc",
        "per_table_brotli_binary_diff.h",
        "per_table_brotli_binary_patch.cc",
        "per_table_brotli_binary_patch.h",
    ],
    hdrs = [
        "ift_client.h",
        "per_table_brotli_binary_diff.h",
        "per_table_brotli_binary_patch.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//common",
        "//ift/proto",
        "//ift/proto:IFT_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
        "@iftb//:merger",
    ],
)

cc_test(
    name = "client_test",
    size = "small",
    srcs = [
        "ift_client_test.cc",
        "iftb_binary_patch_test.cc",
        "integration_test.cc",
        "per_table_brotli_binary_diff_test.cc",
        "per_table_brotli_binary_patch_test.cc",
    ],
    copts = [
        "-DHB_EXPERIMENTAL_API",
    ],
    data = [
        "//ift:testdata",
        "//patch_subset:testdata",
    ],
    deps = [
        ":client",
        "//common",
        "//ift/encoder",
        "//ift/proto",
        "//ift/proto:IFT_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@gtest//:gtest_main",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**"]),
    visibility = [
        "//ift:__subpackages__",
        "//common:__subpackages__",
    ],
)
