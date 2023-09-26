load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")


proto_library(
    name = "IFT_proto",
    srcs = ["IFT.proto"],
)

cc_proto_library(
    name = "IFT_cc_proto",
    visibility = [
        "//patch_subset:__pkg__",
    ],
    deps = [":IFT_proto"],
)

