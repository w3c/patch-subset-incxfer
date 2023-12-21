cc_library(
    name = "common",
    srcs = [
        "bit_input_buffer.cc",
        "bit_input_buffer.h",
        "bit_output_buffer.cc",
        "bit_output_buffer.h",
        "brotli_binary_diff.cc",
        "brotli_binary_patch.cc",
        "file_font_provider.cc",
        "font_helper.cc",
        "hb_set_unique_ptr.cc",
        "sparse_bit_set.cc",
        "axis_range.cc",
        "indexed_data_reader.h",
    ],
    hdrs = [
        "binary_diff.h",
        "binary_patch.h",
        "branch_factor.h",
        "brotli_binary_diff.h",
        "brotli_binary_patch.h",
        "file_font_provider.h",
        "font_data.h",
        "font_helper.h",
        "font_provider.h",
        "hb_set_unique_ptr.h",
        "sparse_bit_set.h",
        "axis_range.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//brotli:shared_brotli_encoder",
        "@brotli//:brotlidec",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_library(
    name = "mocks",
    srcs = [
        "mock_binary_diff.h",
        "mock_binary_patch.h",
        "mock_font_provider.h",
    ],
    hdrs = [
        "mock_binary_diff.h",
        "mock_binary_patch.h",
        "mock_font_provider.h",
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
        "bit_buffer_test.cc",
        "bit_input_buffer_test.cc",
        "bit_output_buffer_test.cc",
        "branch_factor_test.cc",
        "brotli_patching_test.cc",
        "axis_range_test.cc",
        "indexed_data_reader_test.cc",
        "file_font_provider_test.cc",
        "font_helper_test.cc",
        "sparse_bit_set_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
        "//ift:testdata",
    ],
    deps = [
        ":common",
        ":mocks",
        "@gtest//:gtest_main",
        "@com_google_absl//absl/container:btree",
    ],
)
