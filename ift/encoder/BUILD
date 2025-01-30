cc_library(
  name = "encoder",
  srcs = [
    "encoder.h",
    "encoder.cc",
    "glyph_segmentation.h",
    "glyph_segmentation.cc"
  ],
  deps = [
    "//ift/proto",
    "//ift",
    "@abseil-cpp//absl/container:flat_hash_map",
    "@abseil-cpp//absl/container:flat_hash_set",
    "@abseil-cpp//absl/container:btree",
    "@abseil-cpp//absl/flags:flag",
    "@abseil-cpp//absl/flags:parse",    
    "@harfbuzz",
  ],
  copts = [
    "-DHB_EXPERIMENTAL_API",
  ],
  visibility = [
    "//util:__pkg__",
    "//ift:__pkg__",
    "//ift/client:__pkg__",
    "//js_client:__pkg__",
  ],
)

cc_test(
  name = "encoder_test",
  size = "small",
  srcs = [
    "encoder_test.cc",
  ],
  data = [
    "//ift:testdata",
    "//common:testdata",
  ],
  deps = [
    "//ift/client:fontations",
    "//ift:test_segments",
    ":encoder",
     "@googletest//:gtest_main",
     "//common",
  ],
)