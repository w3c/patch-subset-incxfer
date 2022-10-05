cc_library(
    name = "encoding",
    srcs = [
        # TODO "shared_brotli_encoder.cc",
        "brotli_bit_buffer.cc",
        "brotli_font_diff.cc",
        "brotli_stream.cc",
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
        "//patch_subset:common",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "shared_brotli_encoder",
    hdrs = [
        "shared_brotli_encoder.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@brotli//:brotlienc",
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
        "//common",
        "//patch_subset:client",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
    ],
)
