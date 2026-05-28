# Integrating Distribution Recovery Plan

## Situation

`origin/master` and `origin/net-lockstep-experiment` diverged at commit
`051c28e` from 2026-05-15.

`origin/master` contains the current Gubsy/Splonks integration:

- Gubsy-owned title shell and SDL frame.
- Splonks in-game menu driven through Gubsy.
- Gubsy direct lobby host/join and room browser callbacks.
- Smoke coverage for direct lobby host/join through the actual menu screens.

`origin/net-lockstep-experiment` contains the distribution/onboarding work:

- SDL3 package modes.
- Desktop developer setup scripts and docs.
- Linux/macOS/Windows package scripts and verifiers.
- Android scaffold, SDK setup, APK/AAB build scripts, and runtime smoke helpers.
- iOS simulator/device/archive/upload scaffolding.
- Manual/tag-only release workflow policy.
- Validation evidence bundling, handoff, and status scripts.
- A pause/resume checkpoint for later platform validation work.

A direct merge is unsafe because the distribution branch was based on the old
pre-Gubsy-shell app shape. A full merge would remove or unwind files required by
the current menu integration, including `src/gubsy_shell.cpp`,
`src/gubsy_shell_input.cpp`, `src/gubsy_shell_smoke.cpp`, and the Gubsy-owned
startup path in `src/main.cpp`.

This branch, `integrating-distribution`, is based on `origin/master`. Its goal
is to recover the distribution work while preserving the Gubsy shell/menu work
as the authoritative runtime path.

## Non-Negotiables

- Do not delete or replace the Gubsy shell integration from `origin/master`.
- Do not remove Gubsy shell smoke tests.
- Do not restore SDL2 shim compatibility.
- Keep Splonks and Gubsy on real SDL3 headers and libraries.
- Keep CI/release packaging manual or tag-driven only. Normal pushes should not
  force developers to wait on expensive release builds.
- Keep Gubsy as a dependency handled by Splonks build setup, not a separate
  concept new Splonks developers have to understand deeply.

## Recovery Strategy

### 1. Safe File Port

Copy distribution-only files from `origin/net-lockstep-experiment` onto this
branch:

- `.github/workflows/package.yml`
- `android/**`
- Distribution and platform validation docs under `docs/`
- Android, iOS, macOS, package, validation, bootstrap, and setup scripts under
  `scripts/`
- `CMakePresets.json`, `.gitignore`, and top-level doc updates

These files are mostly additive and should not interfere with runtime behavior.

### 2. Manual Build-System Integration

Patch `CMakeLists.txt` by hand. Keep the current master source list and Gubsy
integration, then add only the distribution-support pieces:

- `SPLONKS_MODE` cache value with `developer` and `release`.
- `SPLONKS_FETCH_DEPS` cache option.
- `SPLONKS_BUNDLE_VERSION`.
- iOS signing and bundle metadata cache values.
- Pinned FetchContent commit SHAs for SDL3, SDL3_ttf, SDL3_image, SDL3_mixer,
  and ImGui.
- Android builds produce `libmain.so` through `add_library(... SHARED ...)`.
- Windows links `ws2_32`.
- iOS bundle target properties and content copy step.

Do not copy the distribution branch source list directly, because that branch
omits the Gubsy shell sources.

### 3. Script Integration

Patch `scripts/build.sh` and `scripts/run.sh` to recover package preset support
without breaking the simple developer path:

- `./scripts/build.sh` remains the default Linux developer build command.
- `SPLONKS_PRESET=dev ./scripts/build.sh` builds the debug developer preset.
- Package presets select their expected build directories.
- Build parallelism uses `SPLONKS_BUILD_JOBS` or CMake defaults.
- `scripts/run.sh` resolves the repo root robustly and runs `.exe` on Windows.

### 4. Platform Source Integration

Port only source changes that are actually needed for distribution/platform
support:

- Windows UDP transport support from `5f2a0dd`.
- MinGW nonblocking warning cleanup from `89d99cd`.
- Android smoke CLI logging hook from `f6c330c`, if still compatible.
- Strict warning cleanup from `687ed6c`, if still relevant.

Do not port broad `src/main.cpp`, state, input, render, or gameplay deltas from
the distribution branch unless a concrete packaging script requires them and the
change is reconciled with the Gubsy shell path.

### 5. Documentation Reconciliation

Update imported docs where necessary so they describe this repaired branch
truthfully:

- Developer onboarding is desktop-first.
- Linux/macOS/Windows dev setup is the short-term priority.
- Android/iOS are scaffolded but need real external validation later.
- Validation evidence from the old branch is stale unless regenerated on this
  branch's commit.
- Release builds are manual/tag-driven, not push-driven.

### 6. Validation

Minimum validation before pushing this branch:

- `./scripts/build.sh`
- `./build/splonks-cpp --check-gubsy-shell-smoke`
- `./scripts/verify_dev_env.sh` if available and compatible
- `./scripts/validation_status.sh` if it can run locally without external
  platform credentials

Optional validation after the minimum passes:

- `./scripts/package_linux.sh`
- `./scripts/verify_package_linux.sh`
- `SPLONKS_PRESET=dev ./scripts/build.sh`

macOS, Windows, Android, and iOS runtime/release validation remain external
handoff work unless those machines/toolchains are available.

## Success Criteria

This branch is ready for review when:

- Gubsy in-game menu integration from `origin/master` still builds.
- Direct lobby/menu smoke coverage still exists and passes locally.
- Distribution docs/scripts from `origin/net-lockstep-experiment` are present.
- SDL3 packaging modes and dependency fetching are present without SDL2 shims.
- The branch no longer risks deleting Gubsy shell files during integration.
- Remaining platform validation gaps are documented as explicit handoff work.
