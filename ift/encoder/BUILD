cc_library(
  name = "encoder",
  srcs = [
    "encoder.h",
    "encoder.cc",
    "iftb_patch_creator.h",
    "iftb_patch_creator.cc",
  ],
  deps = [
    "//patch_subset:common",
    "//ift/proto:IFT_cc_proto",
    "//ift/proto",
    "//ift:client",
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
    "//js_client:__pkg__",
  ],
)

cc_test(
  name = "encoder_test",
  size = "small",
  srcs = [
    "encoder_test.cc",
    "iftb_patch_creator_test.cc",
  ],
  data = [
    "//ift:testdata",
    "//patch_subset:testdata",
  ],
  deps = [
    ":encoder",
    "//patch_subset:client",
     "@gtest//:gtest_main",
     "//common",
  ],
)