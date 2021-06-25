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

py_library(
    name = "slicing_strategy_loader",
    srcs = [
        "slicing_strategy_loader.py",
    ],
    data = [
        ":slicing_strategies",
    ],
    visibility = [
        "//analysis/pfe_methods:__pkg__",
    ],
    deps = [
        ":slicing_strategy_py_proto",
        "@fonttools",
    ],
)

py_test(
    name = "slicing_strategy_loader_test",
    srcs = [
        "slicing_strategy_loader_test.py",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        ":slicing_strategy_loader",
    ],
)
