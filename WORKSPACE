workspace(name = "w3c_patch_subset_incxfer")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

## To create compile commands json for clangd integration:
#   Hedron's Compile Commands Extractor for Bazel
#   https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",

    # Replace the commit hash in both places (below) with the latest, rather than using the stale one here.
    # Even better, set up Renovate and let it do the work for you (see "Suggestion: Updates" in the README).
    sha256 = "3cd0e49f0f4a6d406c1d74b53b7616f5e24f5fd319eafc1bf8eee6e14124d115",
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/3dddf205a1f5cde20faf2444c1757abe0564ff4c.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-3dddf205a1f5cde20faf2444c1757abe0564ff4c",
)
load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()

# Base64
http_archive(
    name = "base64",
    build_file = "//third_party:base64.BUILD",
    sha256 = "c101f3ff13ce48d6b80b4c931eb9759ee48812ba2e989c82322d8abd5c8bbcf7",
    strip_prefix = "cpp-base64-82147d6d89636217b870f54ec07ddd3e544d5f69",
    url = "https://github.com/ReneNyffenegger/cpp-base64/archive/82147d6d89636217b870f54ec07ddd3e544d5f69.zip",
)

# Base32hex

http_archive(
    name = "cppcodec",
    build_file = "//third_party:cppcodec.BUILD",
    integrity = "sha256-abpzBt/WJEKA0os255vhquwnQd2njfw6RryLFWsMRU0=",
    strip_prefix = "cppcodec-8019b8b580f8573c33c50372baec7039dfe5a8ce",
    url = "https://github.com/tplgy/cppcodec/archive/8019b8b580f8573c33c50372baec7039dfe5a8ce.zip",
)

# Google Test
http_archive(
    name = "gtest",
    sha256 = "24564e3b712d3eb30ac9a85d92f7d720f60cc0173730ac166f27dda7fed76cb2",
    strip_prefix = "googletest-release-1.12.1",
    url = "https://github.com/google/googletest/archive/release-1.12.1.zip",
)

# Brotli Encoder/Decoder
http_archive(
    name = "brotli",
    build_file = "//third_party:brotli.BUILD",
    sha256 = "3b90c83489879701c05f39b68402ab9e5c880ec26142b5447e73afdda62ce525",
    strip_prefix = "brotli-71fe6cac061ac62c0241f410fbd43a04a6b4f303",
    url = "https://github.com/google/brotli/archive/71fe6cac061ac62c0241f410fbd43a04a6b4f303.zip",
)

# WOFF2 Encoder/Decoder
http_archive(
    name = "woff2",
    build_file = "//third_party:woff2.BUILD",
    sha256 = "730b7f9de381c7b5b09c81841604fa10c5dd67628822fa377b776ab7929fe18c",
    strip_prefix = "woff2-c8c0d339131e8f1889ae8aac0075913d98d9a722",
    url = "https://github.com/google/woff2/archive/c8c0d339131e8f1889ae8aac0075913d98d9a722.zip",
)

# harfbuzz
http_archive(
    name = "harfbuzz",
    build_file = "//third_party:harfbuzz.BUILD",
    integrity = "sha256-g1JYKXX8SKTUjiD1irq4oxqdp6Au3xEHyd+Uh/+3c0s=",
    strip_prefix = "harfbuzz-3de224b4883a64c5446267bda1d649db81137afc",
    urls = ["https://github.com/harfbuzz/harfbuzz/archive/3de224b4883a64c5446267bda1d649db81137afc.zip"],
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
    sha256 = "914dffffeb36742c42eaa9f5d10197ea67ab8b19f034f0a98acf975ea10e0989",
    strip_prefix = "abseil-cpp-8c488c44d893e23b043fa081a4e213a3b9441433",
    urls = ["https://github.com/abseil/abseil-cpp/archive/8c488c44d893e23b043fa081a4e213a3b9441433.zip"],
)

http_archive(
  name = "bazel_skylib",
  urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz"],
  sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
)

### Emscripten ###

http_archive(
    name = "emsdk",
    strip_prefix = "emsdk-3.1.44/bazel",
    sha256 = "cb8cded78f6953283429d724556e89211e51ac4d871fcf38e0b32405ee248e91",
    url = "https://github.com/emscripten-core/emsdk/archive/3.1.44.tar.gz",
)

load("@emsdk//:deps.bzl", emsdk_deps = "deps")
emsdk_deps()

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
emsdk_emscripten_deps(emscripten_version = "3.1.44")

load("@emsdk//:toolchains.bzl", "register_emscripten_toolchains")
register_emscripten_toolchains()


### End Emscripten ###

# IFTB - Binned Incremental Font Transfer

http_archive(
    name = "iftb",
    build_file = "//third_party:iftb.BUILD",
    integrity = "sha256-3c4tnaKLikDSVfIvrPSZOFaiLCBIFpeq58foHChScn8=",
    strip_prefix = "binned-ift-reference-829549eb68e6b9f215ac58a61b39c0c12d35e61f",
    urls = ["https://github.com/garretrieger/binned-ift-reference/archive/829549eb68e6b9f215ac58a61b39c0c12d35e61f.zip"],
)

http_archive(
    name = "ift_spec",
    build_file = "//third_party:ift_spec.BUILD",
    sha256 = "3c8e7f78c49272b89b878a61729b1863b9f37c722f6623ee2eb146adccb41333",
    strip_prefix = "IFT-01037d264f657f9164f9522b8b16a7bab2e6917c",
    urls = ["https://github.com/w3c/IFT/archive/01037d264f657f9164f9522b8b16a7bab2e6917c.zip"],
)

# libcbor
http_archive(
    name = "libcbor",
    build_file = "//third_party:libcbor.BUILD",
    sha256 = "dd04ea1a7df484217058d389e027e7a0143a4f245aa18a9f89a5dd3e1a4fcc9a",
    strip_prefix = "libcbor-0.8.0",
    urls = ["https://github.com/PJK/libcbor/archive/refs/tags/v0.8.0.zip"],
)

# uritemplate-cpp
http_archive(
    name = "uritemplate-cpp",
    build_file = "//third_party:uritemplate-cpp.BUILD",
    strip_prefix = "uritemplate-cpp-1.0.1",
    integrity = "sha256-XwiP9k9mGukrqab4M1IfVKPEE55FmPOeLiTN3kxZfaw=",
    urls = ["https://github.com/returnzero23/uritemplate-cpp/archive/v1.0.1.zip"],
)


### RUST ####

# Fontations

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "fontations",
    urls = ["https://github.com/googlefonts/fontations/archive/3bb9549edcdf626e0df76d13c3e2f94357d7ca60.zip"],
    strip_prefix = "fontations-3bb9549edcdf626e0df76d13c3e2f94357d7ca60",
    build_file = "//third_party:fontations.BUILD",
    integrity = "sha256-7pNhLisjWW7IUVfHCgrKnuN58yoTx8aELO7enUPCsq0=",
)

http_archive(
    name = "rules_rust",
    integrity = "sha256-+ETFhat8dugqC27Vq9WVUIud2AdBGerXW8S8msBSU6E=",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.55.0/rules_rust-0.55.0.tar.gz"],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

rules_rust_dependencies()

rust_register_toolchains(
    edition = "2021",
)

load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")

crate_universe_dependencies()

load("@rules_rust//crate_universe:defs.bzl", "crates_repository")

crates_repository(
    name = "fontations_deps",
    cargo_lockfile = "//fontations:Cargo.lock",
    manifests = [
     "@fontations//:Cargo.toml",
     "@fontations//:fauntlet/Cargo.toml",
     "@fontations//:font-codegen/Cargo.toml",
     "@fontations//:font-test-data/Cargo.toml",
     "@fontations//:font-types/Cargo.toml",
     "@fontations//:fuzz/Cargo.toml",
     "@fontations//:incremental-font-transfer/Cargo.toml",
     "@fontations//:klippa/Cargo.toml",
     "@fontations//:otexplorer/Cargo.toml",
     "@fontations//:read-fonts/Cargo.toml",
     "@fontations//:shared-brotli-patch-decoder/Cargo.toml",
     "@fontations//:skrifa/Cargo.toml",
     "@fontations//:write-fonts/Cargo.toml",
    ],
)

load("@fontations_deps//:defs.bzl", fontations_deps = "crate_repositories")

fontations_deps()


