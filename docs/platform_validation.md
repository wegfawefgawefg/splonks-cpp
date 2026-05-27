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

Gubsy must stay hidden behind the Splonks setup/build path for normal Splonks
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
For desktop releases, `./scripts/verify_release_archive.sh <platform>` also
extracts the archive into a temporary directory and runs the packaged smoke path
from that extracted copy on the matching host platform.

After a platform validator has run the requested validation scopes, bundle the
logs, manifests, and checksums with:

```bash
./scripts/bundle_validation_evidence.sh <platform-or-release-label>
```

The bundle is written under `dist/validation-bundles/` and is the file to send
back to the team for review.
For final release handoff, include distributable artifacts too:

```bash
./scripts/bundle_validation_evidence.sh --include-artifacts <platform-or-release-label>
```

Evidence bundles include `BUNDLE_MANIFEST.txt` with the intended release
version and `CHECKSUMS.sha256` for every included log, manifest, checksum file,
and optional artifact. The import helper verifies `CHECKSUMS.sha256` before
copying evidence into the local `dist/` tree. The status helper also verifies
the latest evidence bundle has the expected release version and valid
`CHECKSUMS.sha256`.
Bundles include only validation logs for the current Git revision and selected
`SPLONKS_RELEASE_VERSION` by default. If a validator intentionally needs a
diagnostic bundle with stale logs, set `SPLONKS_BUNDLE_ALLOW_STALE=1`; those
bundles should not be used as release completion evidence.

When receiving a bundle from another machine, import it with:

```bash
./scripts/import_validation_evidence.sh path/to/splonks-validation-*.tar.gz
```

Then rerun `./scripts/validation_status.sh`.
The status script verifies release artifact SHA-256 values against imported
`.sha256` files or package manifests; a present artifact with a mismatched hash
does not count as release evidence.
Imports keep validation logs additive, and overwrite generated manifests,
checksums, and release artifacts with the versions from the latest imported
bundle. This lets a validator rerun a platform and send a corrected bundle
without the receiver manually clearing stale `dist/` files first.

To inspect the current evidence state, run:

```bash
./scripts/validation_status.sh
```

Use `./scripts/validation_status.sh --strict` as the completion gate; it exits
nonzero until every desktop, Android, iOS, handoff, and manual/tag CI evidence
item has been recorded.
Set `SPLONKS_RELEASE_VERSION=<version>` when auditing evidence for a version
other than `0.1.0`.
By default, status evidence must also match the current Git revision. Set
`SPLONKS_VALIDATION_REVISION=<short commit>` only when intentionally auditing a
specific older release commit.

Use [desktop_validation_handoff.md](desktop_validation_handoff.md) when sending
macOS or Windows validation tasks to another developer.
To print a copy/paste handoff for one external validator, run:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh macos
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh windows
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh android-play
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh ios
```

The handoff generator prints the exact release version, remote ref, and commit
revision being audited. By default it uses the current branch and current HEAD.
For a tagged release or an older commit, set both values explicitly:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 \
SPLONKS_VALIDATION_REF=v0.1.0 \
SPLONKS_VALIDATION_REVISION=<release-commit> \
./scripts/print_validation_handoff.sh all
```

For release credentials and upload prerequisites, validators can run the
preflight helper before a long package/archive/upload command. Upload preflights
also verify that the expected Android AAB or iOS IPA, manifest, checksum, and
bundled release contents are present for the selected release version:

```bash
./scripts/release_credentials_preflight.sh macos-notarized
./scripts/release_credentials_preflight.sh android-release
./scripts/release_credentials_preflight.sh android-play
./scripts/release_credentials_preflight.sh ios-sim
./scripts/release_credentials_preflight.sh ios-release
./scripts/release_credentials_preflight.sh ios-device
./scripts/release_credentials_preflight.sh ios-upload
```

## Linux

Developer validation:

```bash
./scripts/bootstrap_dev.sh --run
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
./scripts/bootstrap_dev.sh --run
```

Evidence helper after setup:

```bash
./scripts/validate_platform.sh dev
```

Release validation on an Apple Silicon Mac:

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
./scripts/validate_platform.sh macos-notarized
```

Expected release artifacts:

```text
dist/splonks-macos/Splonks.app
dist/releases/splonks-0.1.0-macos-arm64.zip
dist/releases/splonks-0.1.0-macos-arm64.zip.sha256
```

Current status: scripts exist. Needs validation on a real Apple Silicon macOS machine,
including app launch from the packaged bundle, Developer ID signing,
notarization, stapling, and launch after download/quarantine.
Package metadata is wired through `SPLONKS_RELEASE_VERSION`,
`SPLONKS_MACOS_BUNDLE_ID`, and `SPLONKS_MACOS_BUNDLE_NAME`.
The `macos-notarized` validator now simulates the download/quarantine case by
extracting the final zip, setting `com.apple.quarantine`, running `spctl`, and
launching the app smoke from the quarantined extracted bundle.

## Windows

Developer validation from the MSYS2 UCRT64 terminal:

```bash
./scripts/bootstrap_dev.sh --run
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
The Windows package and archive verifiers exercise `run-splonks.bat` directly
instead of launching the executable directly.

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
eval "$(./scripts/android/create_validation_keystore.sh)"
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
./scripts/android/verify_release_aab.sh
```

For real Play distribution, use the actual upload keystore instead:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
export SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
./scripts/android/verify_release_aab.sh
```

Evidence helper:

```bash
./scripts/validate_platform.sh android-release
```

Play internal testing upload validation:

```bash
export SPLONKS_PLAY_SERVICE_ACCOUNT_JSON=/absolute/path/to/google-play-service-account.json
export SPLONKS_ANDROID_PACKAGE_NAME=dev.splonks.game
export SPLONKS_PLAY_TRACK=internal
export SPLONKS_PLAY_RELEASE_STATUS=draft
./scripts/validate_platform.sh android-play-upload
```

This upload validation requires current `android-release` evidence from the real
upload key before it runs Fastlane.

Expected release artifacts:

```text
dist/splonks-android/splonks-0.1.0-android-release.aab
dist/splonks-android/manifest.txt
```

Current status: SDK setup, SDL3 AAR fetch, x86_64 debug APK build, emulator
install/runtime smoke, signed arm64 release AAB, and AAB artifact verification
have passed locally with a throwaway validation keystore. Final distribution
still needs `SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload` evidence from the real
upload key and Play Console upload validation. See
[android_play_release.md](android_play_release.md) for the exact Play Console
handoff and evidence to record.

## iOS

Simulator validation on macOS/Xcode:

```bash
./scripts/release_credentials_preflight.sh ios-sim
./scripts/ios/build_sim.sh
./scripts/ios/run_sim.sh --check-state-fingerprint-smoke
```

Evidence helper:

```bash
./scripts/release_credentials_preflight.sh ios-sim
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
./scripts/ios/verify_release_ipa.sh
```

For CI or any other non-interactive machine, import signing assets first:

```bash
export SPLONKS_IOS_CERTIFICATE_BASE64=<base64 p12>
export SPLONKS_IOS_CERTIFICATE_PASSWORD=<p12 password>
export SPLONKS_IOS_PROVISIONING_PROFILE_BASE64=<base64 mobileprovision>
export SPLONKS_IOS_KEYCHAIN_PASSWORD=<temporary keychain password>
./scripts/ios/import_signing_assets.sh
```

Evidence helper:

```bash
./scripts/validate_platform.sh ios-release
```

Physical device install/launch validation after archive/export:

```bash
export SPLONKS_IOS_DEVICE_ID=<device-id-from-xcrun-devicectl-list-devices>
./scripts/release_credentials_preflight.sh ios-device
./scripts/validate_platform.sh ios-device
```

App Store Connect/TestFlight upload validation:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_APP_STORE_API_KEY=ABC123DEFG
export SPLONKS_APP_STORE_API_ISSUER=00000000-0000-0000-0000-000000000000
export API_PRIVATE_KEYS_DIR=/absolute/path/to/appstoreconnect/private_keys
./scripts/ios/upload_app_store.sh validate-upload
```

Reference: <https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds>.
The private key file must be named
`AuthKey_<SPLONKS_APP_STORE_API_KEY>.p8`.

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
working local `how-to-multi-backend-rendering` iOS shape. The simulator
validation path now installs, launches, and requires the state-fingerprint
runtime smoke line, and the simulator preflight checks required macOS/Xcode
tools before a full build. The upload helper exists for App Store
Connect/TestFlight delivery, and the IPA verifier checks the exported IPA,
checksum, manifest, bundled content, and macOS `Info.plist` version fields
before upload. Needs macOS/Xcode validation, real
signing/provisioning, physical device install/launch validation through
`scripts/ios/install_device.sh`, and TestFlight/App Store upload validation.

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
