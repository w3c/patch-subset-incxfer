load("@rules_cc//cc:defs.bzl", "cc_binary")

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
        "//patch_subset/cbor",
        "//patch_subset:common",
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
        "//patch_subset/cbor",
        "//patch_subset:common",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "convert-iftb",
    srcs = [
        "convert-iftb.cc",
    ],
    deps = [
        "//patch_subset:common",
        "//patch_subset/proto",
        "//patch_subset/proto:IFT_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "precompress-test",
    srcs = [
        "precompress-test.cc",
    ],
    deps = [
        "//patch_subset:client",
        "//patch_subset:common",
        "//brotli:encoding",
        "@harfbuzz",
    ],
)
