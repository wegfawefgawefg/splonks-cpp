# Developer Setup

This document is the onboarding contract for Splonks contributors. The goal is
that a developer can clone the repo, follow one platform section, build once,
and launch the game locally without waiting on GitHub Actions.

Gubsy is a library/tool dependency. Normal Splonks development should not
require manually packaging Gubsy or understanding Gubsy internals. CMake uses
the repository dependency path configured by Splonks.

Use the `dev` preset for normal contributor work. It builds into `build-debug`
with `SPLONKS_MODE=developer` and fetches the pinned SDL3/imgui dependencies
when they are not already available.

`verify_dev_env.sh` is the non-interactive onboarding check: it configures,
builds, and runs a headless smoke through the dev binary. `run.sh` is the
interactive game launch.

`bootstrap_dev.sh` is the preferred desktop onboarding command. It runs the
supported platform setup script, then runs `verify_dev_env.sh` with the `dev`
preset. Use `./scripts/bootstrap_dev.sh --run` when the onboarding command
should launch the game immediately after the smoke passes.

`validate_platform.sh` is the evidence collector for platform handoff. After
running `bootstrap_dev.sh`, use `./scripts/validate_platform.sh dev` to produce
a timestamped `dist/validation/` log that can be shared back with the team.
For macOS and Windows validator instructions, see
[desktop_validation_handoff.md](desktop_validation_handoff.md).

## Linux

Supported path: Debian/Ubuntu through `apt`.

```bash
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/bootstrap_dev.sh --run
```

The bootstrap script installs the native packages needed to compile SDL3 and its
Linux backends:

```text
build-essential cmake ninja-build pkg-config wayland-protocols
libwayland-dev libxkbcommon-dev libx11-dev libxext-dev libxcursor-dev
libxi-dev libxfixes-dev libxrandr-dev libxrender-dev libxss-dev
libasound2-dev libpulse-dev libpipewire-0.3-dev libdecor-0-dev
libdrm-dev libgbm-dev libudev-dev
```

For non-apt Linux distributions, install the equivalent compiler, CMake, Ninja,
pkg-config, X11, Wayland, audio, udev, DRM/GBM, and libdecor development
packages, then run:

```bash
./scripts/bootstrap_dev.sh --skip-setup --run
```

Status: verified from a fresh Linux clone with no adjacent Gubsy checkout:
`SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh` configures, builds, and passes
the headless smoke.

## macOS

Supported path: Xcode command line tools plus Homebrew.

```bash
xcode-select --install
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/bootstrap_dev.sh --run
```

The bootstrap script runs `setup_macos.sh`, which installs:

```text
cmake ninja pkg-config
```

SDL3, SDL3_image, SDL3_mixer, SDL3_ttf, and imgui are fetched by CMake unless
you intentionally override dependency discovery.

## Windows

Supported path: MSYS2 UCRT64. Use the **MSYS2 UCRT64** terminal, not PowerShell
or cmd.exe.

First install MSYS2 from <https://www.msys2.org/>. In an MSYS2 terminal, update
the base system:

```bash
pacman -Syu
```

If MSYS2 asks you to close the terminal, reopen **MSYS2 UCRT64** and continue:

```bash
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/bootstrap_dev.sh --run
```

The bootstrap script runs `setup_windows_msys2.sh`, which installs:

```text
git
unzip
mingw-w64-ucrt-x86_64-cmake
mingw-w64-ucrt-x86_64-ninja
mingw-w64-ucrt-x86_64-gcc
mingw-w64-ucrt-x86_64-pkgconf
```

Visual Studio is not the supported Windows onboarding path yet. Add it only
after we validate a separate preset and package flow.

For a faster configure-only prerequisite check on any desktop platform:

```bash
./scripts/bootstrap_dev.sh --configure-only
```

## Android

Supported direction: Android SDK/NDK/JDK with the committed Gradle wrapper.

Prerequisites:

- JDK 17 or newer.
- Android command-line tools or Android Studio.
- `ANDROID_SDK_ROOT` set if your SDK is not under `$HOME/Android/Sdk`.

Current debug/dev path:

```bash
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_apk.sh
```

For the default x86_64 emulator smoke path:

```bash
SPLONKS_ANDROID_ABIS=x86_64 ./scripts/android/build_apk.sh
./scripts/android/create_avd.sh
./scripts/android/start_emulator.sh
./scripts/android/install_apk.sh
./scripts/android/run_smoke.sh
```

With an emulator or device connected:

```bash
./scripts/android/install_apk.sh
./scripts/android/run_app.sh
./scripts/android/run_smoke.sh
```

Status: debug APK build and x86_64 emulator smoke are validated locally through
the commands above. The default APK/AAB release ABI is `arm64-v8a`; use
`SPLONKS_ANDROID_ABIS=x86_64` for the emulator smoke path.

Signed release AAB path:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_ANDROID_VERSION_CODE=1
export SPLONKS_ANDROID_VERSION_NAME=0.1.0
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
```

The release script writes the signed app bundle and manifest under
`dist/splonks-android/`. Keep keystores and passwords outside the repo.

Status: signed arm64 release AAB build is validated locally with a throwaway
upload keystore. Final store distribution still needs the real upload key and
Play Console upload validation.

## iOS

Supported direction: macOS and Xcode only.

The local reference repo `/home/vega/Coding/GameDev/how-to-multi-backend-rendering`
has a verified iOS simulator scaffold. The relevant shape is:

```bash
cmake --preset ios-sim
cmake --build --preset ios-sim
```

That preset uses the Xcode generator, `CMAKE_SYSTEM_NAME=iOS`, simulator SDK,
arm64 simulator architecture, bundled SDL, and app bundle metadata.

Simulator build path:

```bash
./scripts/ios/build_sim.sh
```

Simulator install/launch path:

```bash
./scripts/ios/run_sim.sh
```

`run_sim.sh` builds the simulator app, installs it into a booted iPhone
simulator when one exists, or boots the first available iPhone simulator. Set
`SPLONKS_IOS_SIMULATOR_UDID` to target a specific simulator.

The Splonks `ios-sim` preset uses the Xcode generator, `CMAKE_SYSTEM_NAME=iOS`,
`iphonesimulator`, arm64 simulator architecture, SDL3 FetchContent, and an iOS
`.app` bundle. CMake copies `assets/` and `data/` into the app bundle so startup
can use the bundle as its content root.

Status: simulator scaffold exists. We still need:

- macOS/Xcode validation of `./scripts/ios/build_sim.sh`.
- macOS/Xcode validation of `./scripts/ios/run_sim.sh`.
- macOS/Xcode validation of `./scripts/ios/archive_release.sh`.
- TestFlight/App Store upload validation.

## Release Distribution

Normal branch pushes do not produce release artifacts. Real game distribution
is manual or tag-driven only, through the package workflow and local package
scripts.

See [release_distribution.md](release_distribution.md) for the current
per-platform release matrix, commands, and validation status.
See [platform_validation.md](platform_validation.md) for the proof checklist
that decides when a platform is actually ready for developers or release.

Current release package entry points:

```bash
./scripts/package_linux.sh
./scripts/package_macos.sh
./scripts/package_windows.sh
./scripts/archive_release.sh linux|macos|windows
./scripts/android/build_release_aab.sh
./scripts/ios/archive_release.sh
./scripts/ios/verify_release_ipa.sh
```

Release target state:

- Linux: package script bundles the game, assets/data, runtime libraries,
  manifest, and launcher under `dist/splonks-linux`.
- macOS: package script creates `dist/splonks-macos/Splonks.app`, bundles
  assets/data and dylibs, rewrites local dylib refs, and ad-hoc signs locally.
  Real distribution still needs Developer ID signing and notarization.
- Windows: package script creates `dist/splonks-windows` with the executable,
  assets/data, DLLs, manifest, and launcher batch file.
- Android: debug APK build, x86_64 emulator smoke, and signed arm64 release
  AAB path are validated locally. Final store distribution still needs the real
  upload key and Play Console upload validation.
- iOS: simulator build/launch scaffold, device archive/export script, and IPA
  verifier exist. macOS/Xcode validation, real device signing, and
  TestFlight/App Store upload validation remain.
