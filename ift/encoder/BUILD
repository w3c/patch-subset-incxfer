cc_library(
  name = "encoder",
  srcs = [
    "encoder.h",
    "encoder.cc",
  ],
  deps = [
    "//ift/proto",
    "//ift",
    "@com_google_absl//absl/container:flat_hash_map",
    "@com_google_absl//absl/container:flat_hash_set",
    "@com_google_absl//absl/container:btree",
    "@com_google_absl//absl/flags:flag",
    "@com_google_absl//absl/flags:parse",    
    "@harfbuzz",
    "@iftb//:chunk",
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
     "@gtest//:gtest_main",
     "//common",
  ],
)