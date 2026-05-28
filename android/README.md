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
- `scripts/android/build_release_aab.sh` builds a signed release AAB for real
  distribution when upload-key environment variables are present.
- Android setup still requires JDK 17+, Android command-line tools, and the
  SDK/NDK packages installed by `scripts/android/setup_sdk.sh`.
- Runtime smoke has been validated locally on the default x86_64 emulator with
  `scripts/android/run_smoke.sh`.
- Signed arm64 release AAB generation has been validated locally with a
  throwaway upload keystore.
- `scripts/android/create_validation_keystore.sh` creates an ignored throwaway
  JKS upload keystore for local validation.
- `scripts/android/verify_release_aab.sh` verifies the release AAB manifest,
  SHA-256, expected native libraries/assets, and Java signature.
- Final Play Store distribution still needs the real upload key and Play
  Console upload validation.

## Debug Build

```bash
scripts/android/setup_sdk.sh
scripts/android/fetch_sdl3_aar.sh
scripts/android/build_apk.sh
```

With an emulator or device connected:

```bash
scripts/android/install_apk.sh
scripts/android/run_app.sh
scripts/android/run_smoke.sh
```

Full emulator evidence path:

```bash
scripts/validate_platform.sh android-emulator
```

## Signed Release AAB

The release AAB path is intentionally manual/tag-driven. Do not commit
keystores or passwords.

For local validation, generate a throwaway upload keystore under ignored
`dist/local/` and load the printed signing environment:

```bash
eval "$(scripts/android/create_validation_keystore.sh)"
```

For real Play distribution, use the actual upload keystore instead:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
scripts/android/setup_sdk.sh
scripts/android/fetch_sdl3_aar.sh
scripts/android/build_release_aab.sh
scripts/android/verify_release_aab.sh
```

Full local validation path:

```bash
eval "$(scripts/android/create_validation_keystore.sh)"
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
scripts/android/setup_sdk.sh
scripts/android/fetch_sdl3_aar.sh
scripts/android/build_release_aab.sh
scripts/android/verify_release_aab.sh
```

Evidence path:

```bash
scripts/validate_platform.sh android-release
```

Use [docs/android_play_release.md](../docs/android_play_release.md) for the
Play Console internal testing and production handoff after the AAB verifies.
For command-line Play upload validation, use
`scripts/android/upload_play.sh` or the evidence wrapper
`scripts/validate_platform.sh android-play-upload`.

The script writes:

```text
dist/splonks-android/splonks-<version>-android-release.aab
dist/splonks-android/manifest.txt
```

The Gradle release build reads signing from:

```text
SPLONKS_ANDROID_KEYSTORE
SPLONKS_ANDROID_KEYSTORE_PASSWORD
SPLONKS_ANDROID_KEYSTORE_TYPE      optional, default: jks
SPLONKS_ANDROID_KEY_ALIAS
SPLONKS_ANDROID_KEY_PASSWORD
```

Version metadata is optional. `SPLONKS_ANDROID_VERSION_NAME` defaults to
`SPLONKS_RELEASE_VERSION`, then `0.1.0`:

```text
SPLONKS_ANDROID_VERSION_CODE
SPLONKS_ANDROID_VERSION_NAME
SPLONKS_RELEASE_VERSION
```
