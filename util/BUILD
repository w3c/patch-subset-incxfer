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
