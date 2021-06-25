cc_binary(
    name = "patch_subset_session.so",
    srcs = [
        "patch_subset_session.cc",
    ],
    linkshared = 1,
    linkstatic = 1,
    deps = [
        "//common",
        "//patch_subset:client",
        "//patch_subset:common",
        "//patch_subset:server",
    ],
)

cc_test(
    name = "patch_subset_session_test",
    srcs = [
        "patch_subset_session.cc",
        "patch_subset_session.h",
        "patch_subset_session_test.cc",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        "//common",
        "//patch_subset:client",
        "//patch_subset:common",
        "//patch_subset:server",
        "@gtest//:gtest_main",
    ],
)

py_library(
    name = "py",
    srcs = [
        "patch_subset_method.py",
    ],
    data = [
        ":patch_subset_session.so",
    ],
    visibility = [
        "//analysis:__subpackages__",
    ],
    deps = [
        "//analysis:common",
        "//analysis:simulation",
    ],
)

py_test(
    name = "patch_subset_method_test",
    srcs = [
        "patch_subset_method_test.py",
    ],
    data = [
        "//patch_subset:testdata",
    ],
    deps = [
        ":py",
        "@fonttools",
    ],
)
