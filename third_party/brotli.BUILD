#
# This is a modified version of Brotli's BUILD file.
# The only change is removing -Werror from STRICT_C_OPTIONS.
# This was needed because Brotli's build was broken, with this error:
#
# ------------------------------------------------------------------
# c/common/shared_dictionary.c:463:38: warning: argument 4 of type 'const uint8_t *' {aka 'const unsigned char *'} declared as a pointer [-Wvla-parameter]
#   463 |     size_t data_size, const uint8_t* data) {
#       |                       ~~~~~~~~~~~~~~~^~~~
# In file included from c/common/shared_dictionary.c:9:
# bazel-out/k8-fastbuild/bin/_virtual_includes/brotli_inc/brotli/shared_dictionary.h:91:37: note: previously declared as a variable length array 'const uint8_t[data_size]' {aka 'const unsigned char[data_size]'}
#    91 |     size_t data_size, const uint8_t data[BROTLI_ARRAY_PARAM(data_size)]);
#       |                       ~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# ------------------------------------------------------------------
#
# To vefify if this is still an issue, download the zip file (see WORKSPACE),
# and run "bazel build bazel build :brotlicommon". If this builds, then you can
# remove this file, and remove the build_file line in the brotli section in WORKSPACE.
#


# Description:
#   Brotli is a generic-purpose lossless compression algorithm.

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # MIT

exports_files(["LICENSE"])

config_setting(
    name = "darwin",
    values = {"cpu": "darwin"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "darwin_x86_64",
    values = {"cpu": "darwin_x86_64"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows",
    values = {"cpu": "x64_windows"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows_msvc",
    values = {"cpu": "x64_windows_msvc"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "windows_msys",
    values = {"cpu": "x64_windows_msys"},
    visibility = ["//visibility:public"],
)

load(":compiler_config_setting.bzl", "create_msvc_config")

create_msvc_config()

STRICT_C_OPTIONS = select({
    ":msvc": [],
    "//conditions:default": [
        "--pedantic-errors",
        "-Wall",
        "-Wconversion",
        "-Wextra",
        "-Wlong-long",
        "-Wmissing-declarations",
        "-Wmissing-prototypes",
        "-Wno-strict-aliasing",
        "-Wshadow",
        "-Wsign-compare",
    ],
})

filegroup(
    name = "public_headers",
    srcs = glob(["c/include/brotli/*.h"]),
)

filegroup(
    name = "common_headers",
    srcs = glob(["c/common/*.h"]),
)

filegroup(
    name = "common_sources",
    srcs = glob(["c/common/*.c"]),
)

filegroup(
    name = "dec_headers",
    srcs = glob(["c/dec/*.h"]),
)

filegroup(
    name = "dec_sources",
    srcs = glob(["c/dec/*.c"]),
)

filegroup(
    name = "enc_headers",
    srcs = glob(["c/enc/*.h"]),
)

filegroup(
    name = "enc_sources",
    srcs = glob(["c/enc/*.c"]),
)

cc_library(
    name = "brotli_inc",
    hdrs = [":public_headers"],
    copts = STRICT_C_OPTIONS,
    strip_include_prefix = "c/include",
)

cc_library(
    name = "brotlicommon",
    srcs = [":common_sources"],
    hdrs = [":common_headers"],
    copts = STRICT_C_OPTIONS,
    deps = [":brotli_inc"],
)

cc_library(
    name = "brotlidec",
    srcs = [":dec_sources"],
    hdrs = [":dec_headers"],
    copts = STRICT_C_OPTIONS,
    deps = [":brotlicommon"],
)

cc_library(
    name = "brotlienc",
    srcs = [":enc_sources"],
    hdrs = [":enc_headers"],
    copts = STRICT_C_OPTIONS,
    linkopts = select({
        ":msvc": [],
        "//conditions:default": ["-lm"],
    }),
    deps = [":brotlicommon"],
)

cc_binary(
    name = "brotli",
    srcs = ["c/tools/brotli.c"],
    copts = STRICT_C_OPTIONS,
    linkstatic = 1,
    deps = [
        ":brotlidec",
        ":brotlienc",
    ],
)

filegroup(
    name = "dictionary",
    srcs = ["c/common/dictionary.bin"],
)
