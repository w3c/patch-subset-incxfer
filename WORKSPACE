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
    build_file = "//third_party:brotli.BUILD",
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
    sha256 = "932b6dd00d91ebdda3882f371a29f56d55b1bdffc0586c3bf4f5fddb06435ad4",
    strip_prefix = "harfbuzz-3.1.1",
    urls = ["https://github.com/harfbuzz/harfbuzz/archive/3.1.1.zip"],
)

# Fast Hash
http_archive(
    name = "fasthash",
    build_file = "//third_party:fasthash.BUILD",
    sha256 = "0f8fba20ea2b502c2aaec56d850367768535003ee0fc0e56043283db64e483ee",
    strip_prefix = "fast-hash-ae3bb53c199fe75619e940b5b6a3584ede99c5fc",
    urls = ["https://github.com/ztanml/fast-hash/archive/ae3bb53c199fe75619e940b5b6a3584ede99c5fc.zip"],
)

# abseil-cpp
http_archive(
    name = "com_google_absl",
    sha256 = "1a7edda1ff56967e33bc938a4f0a68bb9efc6ba73d62bb4a5f5662463698056c",
    strip_prefix = "abseil-cpp-20210324.2",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20210324.2.zip"],
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

# libcbor
http_archive(
    name = "libcbor",
    build_file = "//third_party:libcbor.BUILD",
    sha256 = "dd04ea1a7df484217058d389e027e7a0143a4f245aa18a9f89a5dd3e1a4fcc9a",
    strip_prefix = "libcbor-0.8.0",
    urls = ["https://github.com/PJK/libcbor/archive/refs/tags/v0.8.0.zip"],
)

# open-vcdiff
http_archive(
    name = "open-vcdiff",
    build_file = "//third_party:open-vcdiff.BUILD",
    sha256 = "39ce3a95f72ba7b64e8054d95e741fc3c69abddccf9f83868a7f52f3ae2174c0",
    strip_prefix = "open-vcdiff-868f459a8d815125c2457f8c74b12493853100f9",
    urls = ["https://github.com/google/open-vcdiff/archive/868f459a8d815125c2457f8c74b12493853100f9.zip"],
)


