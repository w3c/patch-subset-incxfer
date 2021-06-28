load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")

filegroup(
    name = "slicing_strategies",
    srcs = glob(["*.textproto"]),
    visibility = [
        "//patch_subset:__pkg__",
    ],
)

proto_library(
    name = "slicing_strategy_proto",
    srcs = ["slicing_strategy.proto"],
)

py_proto_library(
    name = "slicing_strategy_py_proto",
    srcs = ["slicing_strategy.proto"],
    visibility = [
        "//tools:__pkg__",
    ],
)

cc_proto_library(
    name = "slicing_strategy_cc_proto",
    visibility = [
        "//patch_subset:__pkg__",
    ],
    deps = [":slicing_strategy_proto"],
)

