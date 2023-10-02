cc_library(
    name = "client",
    srcs = [
        "ift_client.cc",
    ],
    hdrs = [
        "ift_client.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//patch_subset:common",
        "//patch_subset/proto",
        "//patch_subset/proto:IFT_cc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@harfbuzz",        
    ],
)

cc_test(
    name = "ift_client_test",
    size = "small",
    srcs = [
        "ift_client_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        ":client",
        "//patch_subset/proto:proto",
        "//patch_subset/proto:IFT_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@gtest//:gtest_main",
    ],
)