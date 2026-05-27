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

## Linux

Supported path: Debian/Ubuntu through `apt`.

```bash
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/setup_linux.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

The setup script installs the native packages needed to compile SDL3 and its
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
SPLONKS_PRESET=dev ./scripts/run.sh
```

## macOS

Supported path: Xcode command line tools plus Homebrew.

```bash
xcode-select --install
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/setup_macos.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

The setup script installs:

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
./scripts/setup_windows_msys2.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

The setup script installs:

```text
git
mingw-w64-ucrt-x86_64-cmake
mingw-w64-ucrt-x86_64-ninja
mingw-w64-ucrt-x86_64-gcc
mingw-w64-ucrt-x86_64-pkgconf
```

Visual Studio is not the supported Windows onboarding path yet. Add it only
after we validate a separate preset and package flow.

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

With an emulator or device connected:

```bash
./scripts/android/install_apk.sh
./scripts/android/run_app.sh
./scripts/android/run_smoke.sh
```

Status: scaffold and scripts exist. Runtime smoke still needs a real
emulator/device validation pass in this repo. Release distribution still needs
the signed AAB path documented and scripted.

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

Status: Splonks iOS scaffold is not implemented yet. We still need:

- `ios-sim` configure/build presets.
- App bundle metadata and asset/data packaging.
- Simulator launch validation.
- Device signing/provisioning docs.
- TestFlight/App Store release path.

## Release Distribution

Normal branch pushes do not produce release artifacts. Real game distribution
is manual or tag-driven only, through the package workflow and local package
scripts.

Current release package entry points:

```bash
./scripts/package_linux.sh
./scripts/package_macos.sh
./scripts/package_windows.sh
./scripts/android/build_apk.sh
```

Release target state:

- Linux: package script bundles the game, assets/data, runtime libraries,
  manifest, and launcher under `dist/splonks-linux`.
- macOS: package script creates `dist/splonks-macos/Splonks.app`, bundles
  assets/data and dylibs, rewrites local dylib refs, and ad-hoc signs locally.
  Real distribution still needs Developer ID signing and notarization.
- Windows: package script creates `dist/splonks-windows` with the executable,
  assets/data, DLLs, manifest, and launcher batch file.
- Android: debug APK build exists. Release AAB/signing path remains to be
  added.
- iOS: simulator scaffold, device signing, archive, and TestFlight/App Store
  path remain to be added.
