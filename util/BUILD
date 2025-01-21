proto_library(
    name = "encoder_config_proto",
    srcs = ["encoder_config.proto"],
)

cc_proto_library(
    name = "encoder_config_cc_proto",
    deps = [":encoder_config_proto"],
)

cc_library(
    name = "helper",
    srcs = [
        "helper.cc",
    ],
    hdrs = [
        "helper.h",
    ],
    deps = [
        "//common",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "iftb2ift",
    srcs = [
        "iftb2ift.cc",
    ],
    deps = [
        ":convert_iftb",
        "//common",
    ],
)

cc_binary(
    name = "font2ift",
    srcs = [
        "font2ift.cc",
    ],
    deps = [
        ":helper",
        "//ift",
        "//ift/encoder",
        "//common",
        ":encoder_config_cc_proto",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "glyphs_in_iftb_patch",
    srcs = [
        "glyphs_in_iftb_patch.cc",
    ],
    deps = [
        "//ift",
        "//common",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "convert_iftb",
    srcs = [
        "convert_iftb.cc",
        "convert_iftb.h",
    ],
    data = [
        "testdata/Roboto-Regular.Awesome.ttf",
        "testdata/convert-iftb-sample.txt",
    ],
    deps = [
        "//ift/proto",
        "//common",
        "@abseil-cpp//absl/container:btree",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@harfbuzz",
    ],
)

cc_test(
    name = "convert_iftb_test",
    size = "small",
    srcs = [
        "convert_iftb_test.cc",
    ],
    deps = [
        ":convert_iftb",
        "@googletest//:gtest_main",
    ],
)
