cc_library(
    name = "client",
    srcs = [
        "iftb_binary_patch.cc",
        "iftb_binary_patch.h",
        "table_keyed_diff.cc",
        "table_keyed_diff.h",
        "ift_client.cc",
    ],
    hdrs = [
        "table_keyed_diff.h",
        "ift_client.h",
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
        "@cppcodec",
        "@harfbuzz",
        "@iftb//:merger",
        "@uritemplate-cpp//:uritemplate",
    ],
)

cc_test(
    name = "client_test",
    size = "small",
    srcs = [
        "table_keyed_diff_test.cc",
        "ift_client_test.cc",
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
        "//common:__subpackages__",
        "//ift:__subpackages__",
    ],
)
