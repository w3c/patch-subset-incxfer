genrule(
   name = "cbor_config",
   srcs = glob(["**"]),
   outs = ["cbor/configuration.h", "cbor/cbor_export.h"],
   cmd = " && ".join([
     # Remember where output should go.
     "INITIAL_WD=`pwd`",
     # Build needed cbor header files.
     "cd `dirname $(location CMakeLists.txt)`",
     "cmake -DCMAKE_BUILD_TYPE=Release -DCBOR_CUSTOM_ALLOC=ON .",
     "cmake --build . --target cbor/configuration.h src/cbor/cbor_export.h",
     "cp src/cbor/cbor_export.h cbor/configuration.h $$INITIAL_WD/$(RULEDIR)/cbor"]),
   visibility = ["//visibility:private"],
)

cc_library(
    name = "cbor",
    srcs = glob([
      "src/**/*.h",
      "src/**/*.c",

    ]) + [
      ":cbor_config",
    ],
    hdrs = [
      "src/cbor.h",
    ],
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
)
