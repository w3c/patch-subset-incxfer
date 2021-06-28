load("//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

DEFAULT_EMSCRIPTEN_LINKOPTS = [
    "--bind",  # Compiles the source code using the Embind bindings to connect C/C++ and JavaScript
    "-s USE_PTHREADS=0",  # Disable pthreads
    "-s ASSERTIONS=0",  # Turn off assertions
    "-s MODULARIZE=1",  # Allows us to manually invoke the initialization of wasm
    "-s EXPORT_NAME=createModule",  # Not used, but good to specify
    "-s SINGLE_FILE=1",  # Pack all webassembly into base64
    "-s FETCH=1",  # Enable fetch API
]

cc_binary(
    name = "patch_subset_wasm.js",
    srcs = [
        "patch_subset_client.cc",
    ],
    linkopts = DEFAULT_EMSCRIPTEN_LINKOPTS,
    deps = [
        "//common",
        "//patch_subset:client",
        "//patch_subset:common",
        "//patch_subset:patch_subset_cc_proto",
        "@com_google_absl//absl/strings",
        "@harfbuzz",
    ],
)
