cc_library(
    name = "ift",
    srcs = [
        "glyph_keyed_diff.cc",
        "glyph_keyed_diff.h",
        "url_template.cc",
        "table_keyed_diff.cc",
        "table_keyed_diff.h",
    ],
    hdrs = [
        "url_template.h",
        "table_keyed_diff.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//common",
        "//ift/proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@cppcodec",
        "@harfbuzz",
        "@iftb//:merger",
        "@uritemplate-cpp//:uritemplate",
    ],
)

cc_library(
    name = "test_segments",
    srcs = [
        "testdata/test_segments.h",
    ],
    hdrs = [
        "testdata/test_segments.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_set",
    ],
    visibility = [
        "//visibility:public",
    ],
)

cc_test(
    name = "ift_test",
    size = "small",
    srcs = [
        "glyph_keyed_diff_test.cc",
        "url_template_test.cc",
        "table_keyed_diff_test.cc",
    ],
    copts = [
        "-DHB_EXPERIMENTAL_API",
    ],
    data = [
        "//ift:testdata",
        "//common:testdata",
    ],
    deps = [
        ":ift",
        "//common",
        "//ift/encoder",
        "//ift/proto",
        "@com_google_absl//absl/container:btree",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "integration_test",
    size = "medium",
    srcs = [
        "integration_test.cc",
    ],
    copts = [
        "-DHB_EXPERIMENTAL_API",
    ],
    data = [
        "//ift:testdata",
        "//common:testdata",
    ],
    deps = [
        ":ift",
        ":test_segments",
        "//common",
        "//ift/encoder",
        "//ift/client:fontations",
        "@com_google_absl//absl/container:flat_hash_set",
        "@gtest//:gtest_main",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**"]),
    visibility = [
        "//common:__subpackages__",
        "//ift:__subpackages__",
    ],
)
