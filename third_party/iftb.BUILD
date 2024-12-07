load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

DEFAULT_EMSCRIPTEN_LINKOPTS = [
    "-s USE_PTHREADS=0",
    "-s ASSERTIONS=0",
    "-s TOTAL_MEMORY=65536000",
    "-s MODULARIZE=1",
    "-s EXPORT_ES6=1",
    "-s SINGLE_FILE=1",
    "-s EXPORTED_RUNTIME_METHODS='[\"AsciiToString\"]'",
    "-s ERROR_ON_UNDEFINED_SYMBOLS=1",
    ("-s EXPORTED_FUNCTIONS=" +
    "_iftb_new_client," +
    "_iftb_delete_client," +
    "_iftb_reserve_initial_font_data," +
    "_iftb_decode_initial_font," +
    "_iftb_has_font," +
    "_iftb_has_failed," +
    "_iftb_get_chunk_count," +
    "_iftb_reserve_unicode_list," +
    "_iftb_reserve_feature_list," +
    "_iftb_compute_pending," +
    "_iftb_get_pending_list_count," +
    "_iftb_get_pending_list_location," +
    "_iftb_range_file_uri," +
    "_iftb_chunk_file_uri," +
    "_iftb_get_chunk_offset," +
    "_iftb_reserve_chunk_data," +
    "_iftb_use_chunk_data," +
    "_iftb_can_merge," +
    "_iftb_merge," +
    "_iftb_get_font_length," +
    "_iftb_get_font_location"),
]

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
    linkopts = DEFAULT_EMSCRIPTEN_LINKOPTS,
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

wasm_cc_binary(
    name = "iftb_wasm",
    cc_target = ":iftb",
)