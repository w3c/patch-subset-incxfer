load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "patch_subset_proto",
    srcs = ["patch_subset.proto"],
)

cc_proto_library(
    name = "patch_subset_cc_proto",
    visibility = [
        "//js_client:__pkg__",
    ],
    deps = [":patch_subset_proto"],
)

cc_library(
    name = "server",
    srcs = [
        "compressed_list_checksum_impl.cc",
        "frequency_codepoint_predictor.cc",
        "harfbuzz_subsetter.cc",
        "noop_codepoint_predictor.h",
        "patch_subset_server_impl.cc",
        "simple_codepoint_mapper.cc",
    ],
    hdrs = [
        "codepoint_mapper.h",
        "codepoint_predictor.h",
        "compressed_list_checksum.h",
        "compressed_list_checksum_impl.h",
        "frequency_codepoint_predictor.h",
        "harfbuzz_subsetter.h",
        "patch_subset_server_impl.h",
        "simple_codepoint_mapper.h",
        "subsetter.h",
    ],
    data = [
        "//analysis/pfe_methods/unicode_range_data:slicing_strategies",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":common",
        ":patch_subset_cc_proto",
        "//analysis/pfe_methods/unicode_range_data:slicing_strategy_cc_proto",
        "//common",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_library(
    name = "client",
    srcs = [
        "brotli_binary_patch.cc",
        "brotli_request_logger.cc",
        "memory_request_logger.cc",
        "patch_subset_client.cc",
    ],
    hdrs = [
        "binary_patch.h",
        "brotli_binary_patch.h",
        "brotli_request_logger.h",
        "memory_request_logger.h",
        "null_request_logger.h",
        "patch_subset_client.h",
        "request_logger.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":common",
        ":patch_subset_cc_proto",
        "//common",
        "@brotli//:brotlidec",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_library(
    name = "constants",
    hdrs = [
        "constants.h",
    ],
    visibility = [
        "//patch_subset:__pkg__",
        "//patch_subset/cbor:__pkg__",
    ],
)

cc_library(
    name = "common",
    srcs = [
        "brotli_binary_diff.cc",
        "codepoint_map.cc",
        "compressed_set.cc",
        "fast_hasher.cc",
        "file_font_provider.cc",
        "hb_set_unique_ptr.cc",
        "sparse_bit_set.cc",
    ],
    hdrs = [
        "binary_diff.h",
        "brotli_binary_diff.h",
        "codepoint_map.h",
        "compressed_set.h",
        "fast_hasher.h",
        "file_font_provider.h",
        "font_data.h",
        "font_provider.h",
        "hasher.h",
        "hb_set_unique_ptr.h",
        "patch_subset_server.h",
        "sparse_bit_set.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":patch_subset_cc_proto",
        "//common",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@fasthash",
        "@harfbuzz",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "brotli_patching_test.cc",
        "brotli_request_logger_test.cc",
        "client_server_integration_test.cc",
        "codepoint_map_test.cc",
        "compressed_list_checksum_impl_test.cc",
        "compressed_set_test.cc",
        "fake_subsetter.h",
        "file_font_provider_test.cc",
        "frequency_codepoint_predictor_test.cc",
        "harfbuzz_subsetter_test.cc",
        "mock_binary_diff.h",
        "mock_binary_patch.h",
        "mock_codepoint_predictor.h",
        "mock_compressed_list_checksum.h",
        "mock_font_provider.h",
        "mock_hasher.h",
        "mock_patch_subset_server.h",
        "patch_subset_client_test.cc",
        "patch_subset_server_impl_test.cc",
        "server_integration_test.cc",
        "simple_codepoint_mapper_test.cc",
        "sparse_bit_set_test.cc",
    ],
    data = [
        ":testdata",
    ],
    deps = [
        ":client",
        ":common",
        ":patch_subset_cc_proto",
        ":server",
        "//common",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
        "@harfbuzz",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**"]),
    visibility = [
        "//visibility:public",
    ],
)
