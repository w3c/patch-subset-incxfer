cc_library(
  name = "common",
  srcs = [
    "font_helper.h",
    "font_helper.cc",
  ],
  visibility = [
    "//visibility:public",
  ],
  deps = [
    "@harfbuzz",
    "@com_google_absl//absl/container:flat_hash_set",
    "@com_google_absl//absl/container:flat_hash_map",
    "@com_google_absl//absl/status:statusor",
    "@com_google_absl//absl/strings",
    "//patch_subset:common",
  ]
)

cc_test(
  name = "common_test",
  size = "small",
  srcs = [
    "font_helper_test.cc",
  ],
  data = [
    "//patch_subset:testdata",
  ],
  deps = [
    ":common",
    "@gtest//:gtest_main",    
  ],
)