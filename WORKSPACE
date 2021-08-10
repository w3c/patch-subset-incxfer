workspace(name = "w3c_patch_subset_incxfer")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Google Test
http_archive(
    name = "gtest",
    sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
    strip_prefix = "googletest-release-1.10.0",
    url = "https://github.com/google/googletest/archive/release-1.10.0.zip",
)

# Brotli Encoder/Decoder
http_archive(
    name = "brotli",
    sha256 = "e96f58fd91ac7691e1d8299cf0c4ca734acbdc8e31b915e1697ff3d303d64e9b",
    strip_prefix = "brotli-19d86fb9a60aa7034d4981b69a5b656f5b90017e",
    url = "https://github.com/google/brotli/archive/19d86fb9a60aa7034d4981b69a5b656f5b90017e.zip",
)

# WOFF2 Encoder/Decoder
http_archive(
    name = "woff2",
    build_file = "//third_party:woff2.BUILD",
    sha256 = "db9ebe2aff6520e22ad9491863fc9e851b71fedbabefbb32508935d0f5cecf91",
    strip_prefix = "woff2-a0d0ed7da27b708c0a4e96ad7a998bddc933c06e",
    url = "https://github.com/google/woff2/archive/a0d0ed7da27b708c0a4e96ad7a998bddc933c06e.zip",
)

# harfbuzz
http_archive(
    name = "harfbuzz",
    build_file = "//third_party:harfbuzz.BUILD",
    sha256 = "35a5ed7fc774644cfd74dc84bbb3e639c3ce2a7a5959d7f138cf8f305c44e97d",
    strip_prefix = "harfbuzz-c08f1b89037b9a0277b8cef67ff2f38bcf253dfd",
    urls = ["https://github.com/harfbuzz/harfbuzz/archive/c08f1b89037b9a0277b8cef67ff2f38bcf253dfd.zip"],
)

# farmhash
http_archive(
    name = "farmhash",
    build_file = "//third_party:farmhash.BUILD",
    sha256 = "470e87745d1393cc2793f49e9bfbd2c2cf282feeeb0c367f697996fa7e664fc5",
    strip_prefix = "farmhash-0d859a811870d10f53a594927d0d0b97573ad06d",
    urls = ["https://github.com/google/farmhash/archive/0d859a811870d10f53a594927d0d0b97573ad06d.zip"],
)

# abseil-cpp
http_archive(
    name = "com_google_absl",
    sha256 = "aa6386de0481bd4a096c25a0dc7ae50c2b57a064abd39f961fb3ce68eda933f8",
    strip_prefix = "abseil-cpp-20200225",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20200225.zip"],
)

# abseil-py
http_archive(
    name = "io_abseil_py",
    sha256 = "e7f5624c861c51901d9d40ebb09490cf728e3bd6133c9ce26059cdc548fc201e",
    strip_prefix = "abseil-py-pypi-v0.9.0",
    urls = ["https://github.com/abseil/abseil-py/archive/pypi-v0.9.0.zip"],
)

# six archive - needed by abseil-py
http_archive(
    name = "six_archive",
    build_file = "@//third_party:six.BUILD",
    sha256 = "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
    strip_prefix = "six-1.10.0",
    urls = [
        "http://mirror.bazel.build/pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
        "https://pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
    ],
)

# Proto buf generating rules
http_archive(
    name = "rules_proto",
    sha256 = "4d421d51f9ecfe9bf96ab23b55c6f2b809cbaf0eea24952683e397decfbd0dd0",
    strip_prefix = "rules_proto-f6b8d89b90a7956f6782a4a3609b2f0eee3ce965",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/f6b8d89b90a7956f6782a4a3609b2f0eee3ce965.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/f6b8d89b90a7956f6782a4a3609b2f0eee3ce965.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()

### Emscripten ###

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "0f2de53628e848c1691e5729b515022f5a77369c76a09fbe55611e12731c90e3",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/2.0.1/rules_nodejs-2.0.1.tar.gz"],
)

load("@build_bazel_rules_nodejs//:index.bzl", "npm_install")

# emscripten 2.0.9
http_archive(
    name = "emscripten",
    sha256 = "42e06a5ad4b369fcb435db097edb8c4fb824b3125a3b8996aca6f10bc79d9dca",
    strip_prefix = "install",
    url = "https://storage.googleapis.com/webassembly/emscripten-releases-builds/linux/d8e430f9a9b6e87502f826c39e7684852f59624f/wasm-binaries.tbz2",
    build_file = "//emscripten_toolchain:emscripten.BUILD",
    type = "tar.bz2",
)

npm_install(
    name = "npm",
    package_json = "@emscripten//:emscripten/package.json",
    package_lock_json = "@emscripten//:emscripten/package-lock.json",
)

# libcbor needs cmake and make
http_archive(
   name = "rules_foreign_cc",
   sha256 = "f294fe98f8b9df1264dfb2b6f73225ce68de3246041e86ccf35d19303eed99d6",
   strip_prefix = "rules_foreign_cc-0.4.0",
   url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.4.0.zip",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

# libcbor
http_archive(
    name = "libcbor",
    build_file = "//third_party:libcbor.BUILD",
    sha256 = "dd04ea1a7df484217058d389e027e7a0143a4f245aa18a9f89a5dd3e1a4fcc9a",
    strip_prefix = "libcbor-0.8.0",
    urls = ["https://github.com/PJK/libcbor/archive/refs/tags/v0.8.0.zip"],
)

