# Javascript Client

## Build
This repository uses the bazel build system, with Web Assembly:

```sh
bazel build --copt=-Os --config=wasm :patch_subset_wasm.js
```

