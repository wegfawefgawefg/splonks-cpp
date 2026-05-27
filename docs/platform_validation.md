# Platform Validation

This is the proof checklist for Splonks developer onboarding and release
distribution. The goal is not only that scripts exist; each supported platform
needs a recorded run showing that a fresh developer can build and launch the
game, and that release artifacts can be produced and verified.

Normal branch pushes must not block on hosted package builds. Use local
validation during development. Use GitHub Actions only for manual or `v*`
tagged release packaging.

## Developer Onboarding Bar

For developer mode, a platform is ready when a new contributor can:

1. Clone `splonks-cpp`.
2. Run the documented setup command for that platform.
3. Run one documented verification command.
4. Launch the game locally.

Gubsy must stay hidden behind the Splonks dependency path for normal Splonks
contributors. They should not need to package Gubsy or understand Gubsy
internals to work on the game.

## Release Distribution Bar

For release mode, a platform is ready when we can produce the real artifact
expected by players or stores and verify it outside the build directory.

Required evidence for every platform:

- OS version and toolchain version.
- Exact commands run.
- Artifact paths.
- SHA-256 checksums for distributable archives or bundles.
- Launch, install, smoke, notarization, or store-upload result as applicable.

Use `./scripts/validate_platform.sh <scope>` to collect a timestamped evidence
log under `dist/validation/`. The setup scripts are still separate because they
may install packages or require platform credentials.

## Linux

Developer validation:

```bash
./scripts/setup_linux.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

Evidence helper after setup:

```bash
./scripts/validate_platform.sh dev
```

Release validation:

```bash
./scripts/package_linux.sh
./scripts/verify_package_linux.sh
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/archive_release.sh linux
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/verify_release_archive.sh linux
```

Evidence helper:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh release
```

Expected release artifacts:

```text
dist/splonks-linux/
dist/releases/splonks-0.1.0-linux-x86_64.tar.gz
dist/releases/splonks-0.1.0-linux-x86_64.tar.gz.sha256
```

Current status: validated locally from a fresh Linux clone with no adjacent
Gubsy checkout. Release package, archive, checksum, and archive verification
have passed locally.

## macOS

Developer validation:

```bash
xcode-select --install
./scripts/setup_macos.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

Evidence helper after setup:

```bash
./scripts/validate_platform.sh dev
```

Release validation:

```bash
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/archive_release.sh macos
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/verify_release_archive.sh macos
```

Evidence helper:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh release
```

Developer ID signing and notarization validation:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_MACOS_BUNDLE_ID=dev.splonks.game
export SPLONKS_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
export SPLONKS_NOTARYTOOL_PROFILE=splonks-notary
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
./scripts/macos/notarize_app.sh
./scripts/verify_release_archive.sh macos
```

Expected release artifacts:

```text
dist/splonks-macos/Splonks.app
dist/releases/splonks-0.1.0-macos-universal.zip
dist/releases/splonks-0.1.0-macos-universal.zip.sha256
```

Current status: scripts exist. Needs validation on a real macOS machine,
including app launch from the packaged bundle, Developer ID signing,
notarization, stapling, and launch after download/quarantine.
Package metadata is wired through `SPLONKS_RELEASE_VERSION`,
`SPLONKS_MACOS_BUNDLE_ID`, and `SPLONKS_MACOS_BUNDLE_NAME`.

## Windows

Developer validation from the MSYS2 UCRT64 terminal:

```bash
./scripts/setup_windows_msys2.sh
SPLONKS_PRESET=dev ./scripts/verify_dev_env.sh
SPLONKS_PRESET=dev ./scripts/run.sh
```

Evidence helper after setup:

```bash
./scripts/validate_platform.sh dev
```

Release validation:

```bash
./scripts/package_windows.sh
./scripts/verify_package_windows.sh
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/archive_release.sh windows
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/verify_release_archive.sh windows
```

Evidence helper:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh release
```

Expected release artifacts:

```text
dist/splonks-windows/
dist/releases/splonks-0.1.0-windows-x86_64.zip
dist/releases/splonks-0.1.0-windows-x86_64.zip.sha256
```

Current status: MSYS2 setup, package, verify, and archive scripts exist. Needs
validation on a real Windows machine, including launch through
`run-splonks.bat` from the extracted release zip.

## Android

Developer/debug validation:

```bash
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
SPLONKS_ANDROID_ABIS=x86_64 ./scripts/android/build_apk.sh
./scripts/android/create_avd.sh
./scripts/android/start_emulator.sh
./scripts/android/install_apk.sh
./scripts/android/run_smoke.sh
```

Evidence helper with an emulator or device already running:

```bash
./scripts/validate_platform.sh android-dev
```

Evidence helper that creates/starts the default emulator:

```bash
./scripts/validate_platform.sh android-emulator
```

Signed release AAB validation:

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
./scripts/android/verify_release_aab.sh
```

Evidence helper:

```bash
./scripts/validate_platform.sh android-release
```

Expected release artifacts:

```text
dist/splonks-android/splonks-0.1.0-android-release.aab
dist/splonks-android/manifest.txt
```

Current status: SDK setup, SDL3 AAR fetch, x86_64 debug APK build, emulator
install/runtime smoke, signed arm64 release AAB, and AAB artifact verification
have passed locally with a throwaway upload keystore. Final distribution still
needs the real upload key and Play Console upload validation.

## iOS

Simulator validation on macOS/Xcode:

```bash
./scripts/ios/build_sim.sh
./scripts/ios/run_sim.sh
```

Evidence helper:

```bash
./scripts/validate_platform.sh ios-sim
```

Device archive/export validation:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_IOS_DEVELOPMENT_TEAM=TEAMID
export SPLONKS_IOS_BUNDLE_ID=dev.splonks.game
export SPLONKS_IOS_CODE_SIGN_IDENTITY="Apple Distribution"
export SPLONKS_IOS_EXPORT_METHOD=app-store-connect
./scripts/ios/archive_release.sh
```

Evidence helper:

```bash
./scripts/validate_platform.sh ios-release
```

App Store Connect/TestFlight upload validation:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_APP_STORE_API_KEY=ABC123DEFG
export SPLONKS_APP_STORE_API_ISSUER=00000000-0000-0000-0000-000000000000
./scripts/ios/upload_app_store.sh validate-upload
```

Reference: <https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds>.

Evidence helper:

```bash
./scripts/validate_platform.sh ios-upload
```

Expected release artifacts:

```text
dist/splonks-ios/Splonks.xcarchive
dist/splonks-ios/export/
dist/splonks-ios/manifest.txt
dist/releases/splonks-0.1.0-ios.ipa
dist/releases/splonks-0.1.0-ios.ipa.sha256
```

Current status: simulator and device archive scaffolds exist, based on the
working local `how-to-multi-backend-rendering` iOS shape. The upload helper
exists for App Store Connect/TestFlight delivery. Needs macOS/Xcode validation,
real signing/provisioning, device install validation, and TestFlight/App Store
upload validation.

## Completion Criteria

This distribution goal is complete only when:

- Linux developer onboarding and release archive validation remain green.
- macOS developer onboarding, packaged app launch, signed/notarized archive,
  and post-download launch are validated.
- Windows developer onboarding, packaged zip verification, and extracted zip
  launch are validated.
- Android debug/emulator smoke and signed release AAB with the real upload key
  are validated, including Play Console upload.
- iOS simulator launch, signed device archive/export, device install, and
  TestFlight/App Store upload are validated.
- GitHub Actions stays manual/tag-only for release packaging.
