load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

cmake(
    name = "AJcbor_cmake",
    #targets = ["all"],
    # Probably this variable should be set by default.
    # Apparently, it needs to be set for shared libraries on Mac OS
    cache_entries = {
        "CMAKE_MACOSX_RPATH": "True",
        "CMAKE_BUILD_TYPE": "Release",
        "CBOR_CUSTOM_ALLOC": "ON",
    },
    lib_source = ":srcs",
    #???out_headers_only = True,
    out_static_libs = [],
    #out_static_libs = ["src/libcbor.a"],
    #out_data_dirs = glob(["**"]),  # Let make see everything.
    out_data_dirs = ["src2"],
    visibility = ["//visibility:private"],
#    visibility = ["//visibility:public"],
)

make(
    name = "AJcbor",
#    env = {
#        "CLANG_WRAPPER": "$(execpath //make_simple/code:clang_wrapper.sh)",
#        "PREFIX": "$$INSTALLDIR$$",
#    },
     #targets = ["all"],
    #lib_source = ":srcs",
    lib_source = ":AJcbor_cmake",
    out_include_dir = "src",
    out_static_libs = ["src/libcbor.a"],
    deps = [":AJcbor_cmake"],
    #visibility = ["//patch_subset/cbor:__pkg__"],
    #visibility = ["//patch_subset/cbor:cbor"],
    visibility = ["//visibility:public"],
)


