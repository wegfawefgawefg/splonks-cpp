# Splonks Android Scaffold

This project uses SDL3's Android AAR/Prefab direction. Put an official SDL3
Android archive in `android/app/libs/`, for example `SDL3-3.4.0.aar`, before
building with Gradle.

The Gradle app owns APK/AAB packaging. CMake builds the native game target and
renames the Android output library to `libmain.so`, which `SplonksActivity`
loads through SDL's Android activity.

Current status:

- Android Gradle project exists.
- `android-arm64` CMake preset exists for native builds.
- Scripts under `scripts/android/` define the dev loop.
- The scaffold has not been validated on an Android SDK/NDK host in this repo.
- Asset loading still needs a real Android runtime pass. Gradle packages repo
  `assets/` and `data/` under those same APK asset prefixes, while existing C++
  code mostly uses filesystem-relative paths.
