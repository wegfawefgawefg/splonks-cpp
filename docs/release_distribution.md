# Release Distribution

Splonks release artifacts are manual or tag-driven. Normal branch pushes should
not make developers wait on hosted package builds.

Gubsy is included as source/tooling through the Splonks build path. Do not ship
or package Gubsy separately for a Splonks game release.

## Linux

Build and verify:

```bash
./scripts/package_linux.sh
./scripts/verify_package_linux.sh
```

Output:

```text
dist/splonks-linux/
```

Current status: verified locally on Linux. The package includes the game
binary, `assets/`, `data/`, bundled SDL runtime libraries, a package manifest,
and `run-splonks.sh`.

## macOS

Build and verify on macOS:

```bash
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
```

Output:

```text
dist/splonks-macos/Splonks.app
```

Current status: package script exists. It creates an app bundle, copies
assets/data, bundles dylibs, rewrites local dylib references, and ad-hoc signs
locally. Real outside-Mac-distribution still needs Developer ID signing,
notarization, and stapling validation.

## Windows

Build and verify from MSYS2 UCRT64 on Windows:

```bash
./scripts/package_windows.sh
./scripts/verify_package_windows.sh
```

Output:

```text
dist/splonks-windows/
```

Current status: package script exists. It copies the executable, assets/data,
DLLs, manifest, and `run-splonks.bat`. It still needs validation on a real
Windows machine after the desktop onboarding cleanup.

## Android

Debug/dev APK:

```bash
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_apk.sh
```

Signed release AAB:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_ANDROID_VERSION_CODE=1
export SPLONKS_ANDROID_VERSION_NAME=0.1.0
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
```

Output:

```text
dist/splonks-android/splonks-<version>-android-release.aab
dist/splonks-android/manifest.txt
```

Current status: signed AAB path is scripted. This Linux host is missing Java and
the Android NDK, so the AAB build and emulator/device runtime smoke still need
validation in an Android-ready environment.

## iOS

Simulator build on macOS/Xcode:

```bash
./scripts/ios/build_sim.sh
```

Current status: `ios-sim` CMake/Xcode scaffold exists and copies `assets/` and
`data/` into the app bundle. It still needs macOS/Xcode configure, build, and
simulator launch validation.

Device/TestFlight/App Store release still needs:

- Apple Developer team selection.
- Signing certificate and provisioning profile setup.
- Archive/export options.
- TestFlight upload path.
- Final App Store distribution checklist.

## GitHub Actions

The package workflow is intentionally limited to manual dispatch and `v*` tags.
Do not add package jobs back to normal branch pushes.
