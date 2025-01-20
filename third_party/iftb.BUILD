cc_binary(
    name = "iftb",
    srcs = [
        "src/wasm_wrapper.cc",
        "src/wasm_wrapper.h",
        "src/client.cc",
        "src/client.h",
        "src/sfnt.cc",
        "src/sfnt.h",
        "src/cmap.cc",
        "src/cmap.h",
        "src/merger.cc",
        "src/merger.h",
        "src/table_IFTB.cc",
        "src/table_IFTB.h",
        "src/streamhelp.h",
        "src/tag.h",
        "src/randtest.h",        
    ],
    copts = [
        "-Wno-reorder-ctor",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-private-field",
    ],
    deps = [
        "@brotli",
        "@woff2", 
    ],
)

cc_library(
    name = "merger",
    srcs = [
        "src/sfnt.cc",
        "src/sfnt.h",
        "src/merger.cc",
        "src/merger.h",
        "src/streamhelp.h",
        "src/tag.h",
        "src/table_IFTB.cc",
        "src/table_IFTB.h",
        "src/cmap.cc",
        "src/cmap.h",
    ],
    hdrs = [
        "src/merger.h",
    ],
    includes = [
        "src/",
    ],
    copts = [
        "-Wno-reorder-ctor",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-private-field",
    ],
    deps = [
        "@brotli",
        "@woff2",
        "@harfbuzz",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "chunk",
    srcs = [
        "src/wrappers.h",
        "src/chunk.cc",
    ],
    hdrs = [
        "src/chunk.h",
    ],
    includes = [
        "src/",
    ],
    copts = [
        "-Wno-reorder-ctor",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-private-field",
    ],
    deps = [
        "@brotli",
        "@woff2",
        "@harfbuzz",
        ":merger",
    ],
    visibility = ["//visibility:public"],
)