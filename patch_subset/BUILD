load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "server",
    srcs = [
        "frequency_codepoint_predictor.cc",
        "harfbuzz_subsetter.cc",
        "noop_codepoint_predictor.h",
        "patch_subset_server_impl.cc",
        "simple_codepoint_mapper.cc",
    ],
    hdrs = [
        "codepoint_mapper.h",
        "codepoint_predictor.h",
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
        "//analysis/pfe_methods/unicode_range_data:slicing_strategy_cc_proto",
        "//common",
        "//patch_subset/cbor",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_library(
    name = "client",
    srcs = [
        "brotli_request_logger.cc",
        "memory_request_logger.cc",
        "patch_subset_client.cc",
        "vcdiff_binary_patch.cc",
    ],
    hdrs = [
        "brotli_request_logger.h",
        "memory_request_logger.h",
        "null_request_logger.h",
        "patch_subset_client.h",
        "request_logger.h",
        "vcdiff_binary_patch.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":common",
        "//common",
        "//patch_subset/cbor",
        "@brotli//:brotlidec",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
        "@open-vcdiff//:vcddec",
    ],
)

cc_library(
    name = "simulation",
    srcs = [
        "simulation.cc",
    ],
    hdrs = [
        "simulation.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":client",
        ":common",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "common",
    srcs = [
        "codepoint_map.cc",
        "compressed_set.cc",
        "fast_hasher.cc",
        "integer_list_checksum_impl.cc",
        "vcdiff_binary_diff.cc",
    ],
    hdrs = [
        "codepoint_map.h",
        "compressed_set.h",
        "encodings.h",
        "fast_hasher.h",
        "hasher.h",
        "integer_list_checksum.h",
        "integer_list_checksum_impl.h",
        "patch_subset_server.h",
        "vcdiff_binary_diff.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//brotli:shared_brotli_encoder",
        "//common",
        "//patch_subset/cbor",
        "@brotli//:brotlienc",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@fasthash",
        "@harfbuzz",
        "@open-vcdiff//:vcdenc",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "brotli_request_logger_test.cc",
        "client_server_integration_test.cc",
        "codepoint_map_test.cc",
        "compressed_set_test.cc",
        "fake_subsetter.h",
        "fast_hasher_test.cc",
        "frequency_codepoint_predictor_test.cc",
        "harfbuzz_subsetter_test.cc",
        "integer_list_checksum_impl_test.cc",
        "mock_codepoint_predictor.h",
        "mock_hasher.h",
        "mock_integer_list_checksum.h",
        "mock_patch_subset_server.h",
        "patch_subset_client_test.cc",
        "patch_subset_server_impl_test.cc",
        "server_integration_test.cc",
        "simple_codepoint_mapper_test.cc",
        "vcdiff_patching_test.cc",
    ],
    data = [
        ":testdata",
    ],
    deps = [
        ":client",
        ":common",
        ":server",
        ":simulation",
        "//common",
        "//common:mocks",
        "//patch_subset/cbor",
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
