# Splonks Android Scaffold

This project uses SDL3's Android AAR/Prefab direction. Run
`scripts/android/fetch_sdl3_aar.sh` to download the pinned official SDL3
Android archive and place `SDL3-3.4.0.aar` in `android/app/libs/` before
building with Gradle.

The Gradle app owns APK/AAB packaging. CMake builds the native game target and
renames the Android output library to `libmain.so`, which `SplonksActivity`
loads through SDL's Android activity.

Current status:

- Android Gradle project exists.
- `android-arm64` CMake preset exists for native builds.
- Scripts under `scripts/android/` define the dev loop.
- `scripts/android/fetch_sdl3_aar.sh` downloads the pinned SDL3 Android AAR
  with checksum verification.
- A Gradle wrapper is committed under `android/` so APK builds do not require a
  global Gradle install.
- GitHub Actions installs the pinned Android SDK/NDK/CMake packages, fetches the
  pinned SDL3 AAR, and runs `scripts/android/build_apk.sh` to build a debug APK.
- Android setup still requires JDK 17+, Android command-line tools, and the
  SDK/NDK packages installed by `scripts/android/setup_sdk.sh`.
- Runtime has not been validated on an Android emulator/device in this repo.
- Asset loading still needs a real Android runtime pass. Gradle packages repo
  `assets/` and `data/` under those same APK asset prefixes, while existing C++
  code mostly uses filesystem-relative paths.
