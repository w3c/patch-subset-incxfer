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
        "brotli_stream.cc",
        "brotli_stream.h",
        "brotli_font_diff.cc",
        "brotli_font_diff_test.cc",
        "brotli_font_diff.h",
        "brotli_stream_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
        "//patch_subset:client",
        "@brotli//:brotlienc",
    ],
)
