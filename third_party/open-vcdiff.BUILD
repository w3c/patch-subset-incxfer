cc_library(
    name = "vcdcom",
    srcs = [
        "src/addrcache.cc",
        "src/codetable.cc",
        "src/logging.cc",
        "src/varint_bigendian.cc",
        "src/zlib/adler32.c",
    ],
    deps = [
      "@w3c_patch_subset_incxfer//third_party/open-vcdiff:config",
    ],
    hdrs = [
        "src/addrcache.h",
        "src/checksum.h",
        "src/codetable.h",
        "src/google/output_string.h",
        "src/logging.h",
        "src/unique_ptr.h",
        "src/varint_bigendian.h",
        "src/vcdiff_defs.h",
        "src/zlib/zconf.h",
        "src/zlib/zlib.h",
    ],
    includes = [
        "src",
        "src/zlib",
    ],
)

cc_library(
    name = "vcddec",
    srcs = [
        "src/decodetable.cc",
        "src/headerparser.cc",
        "src/vcdecoder.cc",
    ],
    hdrs = [
        "src/decodetable.h",
        "src/google/vcdecoder.h",
        "src/headerparser.h",
    ],
    deps = [":vcdcom"],
)

cc_library(
    name = "vcdenc",
    srcs = [
        "src/blockhash.cc",
        "src/encodetable.cc",
        "src/instruction_map.cc",
        "src/jsonwriter.cc",
        "src/vcdiffengine.cc",
        "src/vcencoder.cc",
    ],
    hdrs = [
        "src/blockhash.h",
        "src/compile_assert.h",
        "src/google/format_extension_flags.h",
        "src/google/vcencoder.h",
        "src/google/encodetable.h",
        "src/google/jsonwriter.h",
	"src/google/codetablewriter_interface.h",
        "src/instruction_map.h",
        "src/rolling_hash.h",
        "src/vcdiffengine.h",
    ],
    deps = [":vcdcom"],
    visibility = ["//visibility:public"],
)

