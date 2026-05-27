# CI and Release Policy

Splonks is the shipped game. GitHub Actions should be used for release
packaging, not as the normal development feedback loop.

## Normal development

- Build and test locally before committing.
- Pushes to normal branches should not automatically run package builds.
- Do not wait on GitHub-hosted runners for everyday commits.
- A new developer on Linux, macOS, or Windows should be able to clone Splonks,
  follow one platform-specific setup section, run one build command, and launch
  the game locally.
- Android has a documented SDK/NDK/JDK setup, debug APK, emulator smoke, and
  signed AAB path. iOS has documented simulator/device archive scaffolding, but
  still needs macOS/Xcode/signing validation.

## Onboarding Deliverables

The immediate priority is desktop developer onboarding for the two pending
contributors:

1. Add `docs/dev_setup.md` with exact Linux, macOS, and Windows commands.
2. Add setup scripts for Linux, macOS, and the documented Windows toolchain.
3. Confirm `./scripts/build.sh` works from a clean clone on Linux.
4. Make macOS and Windows docs match the actual supported package-manager and
   compiler paths.
5. Add a quick dev environment verification script or checklist.
6. Then finish Android and iOS as mobile development/release targets.

Gubsy should appear in Splonks developer docs only as source/tooling handled by
the documented Splonks setup path when it is actually needed. Normal Splonks
contributors should not need to clone, understand, or package Gubsy manually.

## Release builds

- The package workflow runs only when manually dispatched or when a version tag
  matching `v*` is pushed.
- Release builds are responsible for platform packages, including Linux, macOS,
  Windows, Android, and iOS. Android signing uses environment-provided upload
  key settings; iOS signing uses the Xcode/provisioning path.
- The package workflow uploads versioned Linux/macOS/Windows release archives
  with SHA-256 files. It also uploads an Android debug APK, and uploads a signed
  Android release AAB when Android signing secrets are configured.
- Android Play and iOS App Store/TestFlight delivery are explicit release
  actions. The package workflow only runs them when a manual input requests it
  or when an intentional tag-release repository variable enables it.
- iOS release work requires an explicit Xcode/signing/provisioning path. The
  package workflow has an opt-in signed IPA job for manual dispatch
  (`include_ios` or `upload_ios_app_store`) or tagged releases with
  `SPLONKS_BUILD_IOS_RELEASE=true`; it does not run on normal branch pushes.
- Use `/home/vega/Coding/GameDev/how-to-multi-backend-rendering` as the local
  iOS scaffold reference. Its working `ios-sim` CMake preset uses the Xcode
  generator, `CMAKE_SYSTEM_NAME=iOS`, simulator sysroot, arm64 simulator arch,
  bundled SDL, and app bundle metadata.
- Splonks now has `ios-sim` and `ios-device` CMake presets,
  `scripts/ios/build_sim.sh`, a simulator runtime smoke path through
  `scripts/ios/run_sim.sh --check-state-fingerprint-smoke`, and
  `scripts/ios/archive_release.sh`. Physical device validation runs through
  `scripts/ios/install_device.sh` after archive/export. CI signing assets can
  be imported with `scripts/ios/import_signing_assets.sh`. These paths must be
  validated on macOS/Xcode before iOS is treated as complete.
  Device distribution still needs real signing/provisioning and
  TestFlight/App Store upload validation.
- Use the full matrix when we are making or validating a real release build.
- Track platform readiness in `docs/platform_validation.md`. That checklist is
  the source of truth for whether developer onboarding and real distribution
  are proven on each target.

## Gubsy dependency

Gubsy is a library/tool dependency. It should be validated locally during normal
development. It should not produce automatic hosted package builds on ordinary
pushes, and it should not be distributed as a separate player-facing artifact
for a Splonks game release.
