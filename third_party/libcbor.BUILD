cc_library(
    name = "cbor",
    srcs = glob([
      "src/**/*.h",
      "src/**/*.c",

    ]),
    deps = [
      "@w3c_patch_subset_incxfer//third_party/libcbor:config",
    ],
    hdrs = [
      "src/cbor.h",
    ],
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
)
