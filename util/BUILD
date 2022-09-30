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
        "brotli_bit_buffer.cc",
        "brotli_bit_buffer.h",
        "precompress-test.cc",
    ],
    deps = [
        "//patch_subset:client",
        "//patch_subset:common",
        "@harfbuzz",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "brotli_bit_buffer_test.cc",
        "brotli_bit_buffer.cc",
        "brotli_bit_buffer.h",
    ],
    deps = [
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
        "@harfbuzz",
    ],
)
