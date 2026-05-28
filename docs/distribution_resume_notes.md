# Distribution Resume Notes

Paused on 2026-05-28.

This is the checkpoint for resuming the SDL3 developer onboarding and release
distribution cleanup later.

## Current Revisions

- Splonks branch: `net-lockstep-experiment`
- Splonks checkpoint: `6c21c29c0fd1`
- Gubsy branch: `master`
- Gubsy checkpoint: `c804f8c2a85b`

## Current State

Gubsy local distribution work is complete for this pass. It is treated as a
code-first library/tooling dependency, not a player-shipped app. Its SDL2/SDL3
shim cleanup is done and guarded by `./scripts/validation_status.sh`.

Splonks has the desktop onboarding and release scaffolding in place:

- Linux dev onboarding: `./scripts/bootstrap_dev.sh --run`
- macOS dev onboarding: Apple Silicon only, Xcode command line tools, Homebrew
- Windows dev onboarding: MSYS2 UCRT64
- Linux release package/archive validation: complete locally
- Android local validation: x86_64 emulator smoke and signed arm64 release AAB
- iOS scaffold: simulator, archive/export, device install, and upload wrappers
- CI/package workflow: manual dispatch and `v*` tags only, no PR/branch waits
- macOS release target: `macos-arm64`; Intel Mac support is intentionally out
  of scope for this distribution pass

The current Splonks status command is:

```bash
./scripts/validation_status.sh
```

At pause time it reports `23 missing evidence item(s)`. Those missing rows are
expected because they require external target machines, store credentials, or a
physical device. They are not local Linux blockers.

## Verified Local Evidence

At Splonks `6c21c29c0fd1`, these paths were validated and imported:

```bash
SPLONKS_SKIP_INTERACTIVE_LAUNCH=1 ./scripts/validate_desktop_handoff.sh
eval "$(./scripts/android/create_validation_keystore.sh)"
./scripts/validate_platform.sh android-release
SPLONKS_ANDROID_ABIS=x86_64 ./scripts/validate_platform.sh android-emulator
./scripts/bundle_validation_evidence.sh --include-artifacts linux-android-current
SPLONKS_RELEASE_VERSION=0.1.0 \
  SPLONKS_VALIDATION_REVISION=6c21c29c0fd1 \
  ./scripts/import_validation_evidence.sh dist/validation-bundles/splonks-validation-linux-android-current-*.tar.gz
```

The imported evidence proves:

- Linux developer build and headless smoke
- Linux release package/archive and package manifest
- Android emulator APK install/runtime smoke
- Android signed release AAB using the validation keystore

## Remaining External Evidence

Resume by collecting/importing bundles for these targets:

- macOS Apple Silicon developer validation
- macOS release package/archive
- macOS Developer ID notarization
- Windows MSYS2 UCRT64 developer validation
- Windows release package/archive
- Android real upload-key AAB validation
- Android Play Console validate/upload
- iOS simulator validation on macOS/Xcode
- iOS signed IPA export
- iOS physical device install
- iOS App Store/TestFlight validate/upload

Generated handoff packets are written under:

```text
dist/validation-handoffs/
```

Regenerate fresh packets after any new commit:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/write_validation_handoff.sh macos
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/write_validation_handoff.sh windows
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/write_validation_handoff.sh android-play
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/write_validation_handoff.sh ios
```

Each packet pins the exact commit, lists the status rows it is expected to
clear, and prints the receiver import command.

## Import Pattern

When a validator sends back a bundle, import with the target check enabled:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 \
  SPLONKS_VALIDATION_REVISION=<handoff-short-revision> \
  SPLONKS_IMPORT_EXPECT_TARGET=<macos|windows|android-play|ios> \
  ./scripts/import_validation_evidence.sh path/to/splonks-validation-*.tar.gz

SPLONKS_RELEASE_VERSION=0.1.0 \
  SPLONKS_VALIDATION_REVISION=<handoff-short-revision> \
  ./scripts/validation_status.sh
```

Do not use `SPLONKS_IMPORT_ALLOW_STALE=1` except for diagnostics.

## Key Docs

- `docs/dev_setup.md`: contributor onboarding contract
- `docs/desktop_validation_handoff.md`: macOS/Windows validator instructions
- `docs/platform_validation.md`: proof checklist and status expectations
- `docs/release_distribution.md`: release artifact matrix and commands
- `docs/ci_release_policy.md`: no default CI waits, manual/tag packaging only
