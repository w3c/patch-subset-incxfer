load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_library(
    name = "helper",
    srcs = [
        "helper.cc",
    ],
    hdrs = [
        "helper.h",
    ],
    deps = [
        "//common",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "checksum",
    srcs = [
        "checksum.cc",
    ],
    deps = [
        "//patch_subset:common",
    ],
)

cc_binary(
    name = "print-request",
    srcs = [
        "print-request.cc",
    ],
    deps = [
        "//common",
        "//patch_subset:common",
        "//patch_subset/cbor",
        "@base64",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "print-client-state",
    srcs = [
        "print-client-state.cc",
    ],
    deps = [
        "//common",
        "//patch_subset:common",
        "//patch_subset/cbor",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "iftb2ift",
    srcs = [
        "iftb2ift.cc",
    ],
    deps = [
        ":convert_iftb",
        "//ift/encoder",
    ],
)

cc_binary(
    name = "font2ift",
    srcs = [
        "font2ift.cc",
    ],
    deps = [
        ":helper",
        "//ift:client",
        "//ift/encoder",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "augment",
    srcs = [
        "augment.cc",
    ],
    deps = [
        ":helper",
        "//ift:client",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "patch",
    srcs = [
        "patch.cc",
    ],
    deps = [
        ":helper",
        "//ift:client",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "convert_iftb",
    srcs = [
        "convert_iftb.cc",
        "convert_iftb.h",
    ],
    data = [
        "testdata/Roboto-Regular.Awesome.ttf",
        "testdata/convert-iftb-sample.txt",
    ],
    deps = [
        "//ift/proto",
        "//ift/proto:IFT_cc_proto",
        "//patch_subset:common",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@harfbuzz",
    ],
)

cc_test(
    name = "convert_iftb_test",
    size = "small",
    srcs = [
        "convert_iftb_test.cc",
    ],
    deps = [
        ":convert_iftb",
        "@gtest//:gtest_main",
    ],
)

cc_binary(
    name = "precompress-test",
    srcs = [
        "precompress-test.cc",
    ],
    deps = [
        "//brotli:encoding",
        "//common",
        "//patch_subset:client",
        "//patch_subset:common",
        "@harfbuzz",
    ],
)
