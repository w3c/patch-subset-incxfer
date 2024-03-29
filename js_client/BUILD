load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

DEFAULT_EMSCRIPTEN_LINKOPTS = [
    "--bind",  # Compiles the source code using the Embind bindings to connect C/C++ and JavaScript
    "-s USE_PTHREADS=0",  # Disable pthreads
    "-s ASSERTIONS=0",  # Turn off assertions
    "-s MODULARIZE=1",  # Allows us to manually invoke the initialization of wasm
    "-s EXPORT_NAME=createModule",  # Not used, but good to specify
    "-s SINGLE_FILE=1",  # Pack all webassembly into base64
    "-s FETCH=1",  # Enable fetch API
    "-s INITIAL_MEMORY=65536000",
]

cc_binary(
    name = "patch_subset_client",
    srcs = [
        "patch_subset_client.cc",
    ],
    linkopts = DEFAULT_EMSCRIPTEN_LINKOPTS,
    deps = [
        "//common",
        "//patch_subset:client",
        "//patch_subset:common",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)

cc_binary(
    name = "ift_client",
    srcs = [
        "ift_client.cc",
        "ift_client.h",
    ],
    copts = [
        "-Wno-unused-function",
    ],
    linkopts = DEFAULT_EMSCRIPTEN_LINKOPTS,
    deps = [
        "//common",
        "//ift:client",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

wasm_cc_binary(
    name = "patch_subset_wasm",
    cc_target = ":patch_subset_client",
)

wasm_cc_binary(
    name = "ift_wasm",
    cc_target = ":ift_client",
)
