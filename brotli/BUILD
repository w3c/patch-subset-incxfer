cc_library(
    name = "encoding",
    srcs = [
        # TODO "shared_brotli_encoder.cc",
        "brotli_bit_buffer.cc",
        "brotli_font_diff.cc",
        "brotli_stream.cc"
    ],
    hdrs = [
        # TODO "shared_brotli_encoder.h",
        "brotli_bit_buffer.h",
        "brotli_font_diff.h",
        "brotli_stream.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@brotli//:brotlienc",
        "@com_google_absl//absl/types:span",
        "//patch_subset:common",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "brotli_bit_buffer_test.cc",
        "brotli_font_diff_test.cc",
        "brotli_stream_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        ":encoding",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
        "//patch_subset:client",
        "@brotli//:brotlienc",
        "//common",
    ],
)
