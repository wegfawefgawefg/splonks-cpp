# Splonks Android Scaffold

This project uses SDL3's Android AAR/Prefab direction. Run
`scripts/android/fetch_sdl3_aar.sh` to download the pinned official SDL3
Android archive and place `SDL3-3.4.0.aar` in `android/app/libs/` before
building with Gradle.

The Gradle app owns APK/AAB packaging. CMake builds the native game target and
renames the Android output library to `libmain.so`, which `SplonksActivity`
loads through SDL's Android activity.

At startup, `SplonksActivity` extracts the APK `assets/` tree into app-private
storage, seeds missing `data/` files without overwriting player settings, and
starts native code with `--project-root <path>`. The C++ entry point switches to
that root before loading relative `assets/...` and `data/...` paths, matching
the desktop package layout.

Current status:

- Android Gradle project exists.
- `android-arm64` CMake preset exists for native builds.
- Scripts under `scripts/android/` define the dev loop.
- `scripts/android/fetch_sdl3_aar.sh` downloads the pinned SDL3 Android AAR
  with checksum verification.
- `scripts/android/run_smoke.sh` launches the installed app with
  `--check-state-fingerprint-smoke` through an SDL activity intent extra and
  verifies the expected success line in logcat.
- A Gradle wrapper is committed under `android/` so APK builds do not require a
  global Gradle install.
- GitHub Actions installs the pinned Android SDK/NDK/CMake packages, fetches the
  pinned SDL3 AAR, and runs `scripts/android/build_apk.sh` to build a debug APK.
- The package workflow uploads the debug APK as `splonks-android-debug-apk`
  after the build succeeds.
- Android setup still requires JDK 17+, Android command-line tools, and the
  SDK/NDK packages installed by `scripts/android/setup_sdk.sh`.
- Runtime smoke scripting exists, but has not been validated on an Android
  emulator/device in this repo.
- Asset extraction is implemented, but still needs a real Android runtime pass
  on an emulator/device to prove launch, rendering, audio, and settings writes.
