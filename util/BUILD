proto_library(
    name = "encoder_config_proto",
    srcs = ["encoder_config.proto"],
)

cc_proto_library(
    name = "encoder_config_cc_proto",
    deps = [":encoder_config_proto"],
)

cc_binary(
    name = "font2ift",
    srcs = [
        "font2ift.cc",
    ],
    deps = [
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
