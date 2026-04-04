# splonks-cpp

Literal C++ port of `splonks-rs`.

The immediate goal is not a reinterpretation. It is a behavior-preserving port with repo layout,
assets, notes, and plans carried forward so we can move file-by-file without losing context.

## Current bootstrap

- C++20 + CMake
- SDL3 window/bootstrap
- strict compile flags enabled in the `dev` preset
- `clang-format` repo config
- VS Code `F5` launch path wired through CMake build tasks

## Build

```bash
bash scripts/build.sh
```

## Run

```bash
bash scripts/run.sh
```

## VS Code

- Build task: `cmake: build (dev)`
- Run task: `run splonks-cpp`
- `F5`: `Run splonks-cpp`

## Porting references

- `assets/`: copied from `splonks-rs/assets`
- `docs/asset-metadata-notes.md`: copied from `splonks-rs/docs`
- `docs/legacy/`: copied root notes from `splonks-rs`
- `plans/legacy/`: copied planning markdown from `splonks-rs`
