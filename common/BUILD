cc_library(
    name = "common",
    srcs = [
        "font_helper.cc",
        "brotli_binary_diff.cc",
        "sparse_bit_set.cc",
        "bit_input_buffer.cc",
        "bit_output_buffer.cc",
        "brotli_binary_patch.cc",
        "hb_set_unique_ptr.cc",
        "bit_input_buffer.h",
        "bit_output_buffer.h",
        "file_font_provider.cc",
    ],
    hdrs = [
        "binary_diff.h",
        "binary_patch.h",
        "branch_factor.h",
        "brotli_binary_diff.h",
        "brotli_binary_patch.h",
        "font_data.h",
        "font_helper.h",
        "hb_set_unique_ptr.h",
        "sparse_bit_set.h",
        "file_font_provider.h",
        "font_provider.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//brotli:shared_brotli_encoder",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/log",
        "@harfbuzz",
        "@brotli//:brotlienc",
        "@brotli//:brotlidec",
    ],
)

cc_library(
    name = "mocks",
    srcs = [
      "mock_binary_diff.h",
      "mock_binary_patch.h",
    ],
    hdrs = [
        "mock_binary_diff.h",
        "mock_binary_patch.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":common",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "common_test",
    size = "small",
    srcs = [
        "font_helper_test.cc",
        "bit_buffer_test.cc",
        "bit_input_buffer_test.cc",
        "bit_output_buffer_test.cc",
        "branch_factor_test.cc",
        "brotli_patching_test.cc",
        "sparse_bit_set_test.cc",
        "file_font_provider_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        ":common",
        ":mocks",
        "@gtest//:gtest_main",
    ],
)
