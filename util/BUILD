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
