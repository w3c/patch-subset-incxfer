load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
#load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

cmake(
    name = "AJcbor",
#    cache_entries = {
#        "CMAKE_MACOSX_RPATH": "True",
#        "CMAKE_BUILD_TYPE": "Release",
#        "CBOR_CUSTOM_ALLOC": "ON",
#    },
    lib_source = ":srcs",
    out_lib_dir = "src",
    out_static_libs = ["libcbor.a"],
    visibility = ["//visibility:public"],
)

