genrule(
  name = "feature_registry_h",
  srcs = [
    "@ift_spec//:feature-registry.csv",
  ],
  outs = [
    "feature_registry.h",
  ],
  cmd = "./$(location registry_to_cc) $(location @ift_spec//:feature-registry.csv) > \"$@\"",
  tools = [
    ":registry_to_cc",
  ],
)

cc_library(
  name = "feature_registry",
  srcs = [
    ":feature_registry_h",
  ],
  deps = [
    "@com_google_absl//absl/container:flat_hash_map",
    "@com_google_absl//absl/status:statusor",
    "@com_google_absl//absl/base:no_destructor",
    "@harfbuzz",
  ],
  visibility = [
    "//ift/proto:__pkg__",
  ],
)

cc_test(
  name = "feature_registry_test",
  srcs = [
    "feature_registry_test.cc",
  ],
  deps = [
    ":feature_registry",
    "@gtest//:gtest_main",
  ],
)

py_binary(
  name = "registry_to_cc",
  srcs = [
    "registry_to_cc.py",
  ],
)