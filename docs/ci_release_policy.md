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
- Android and iOS need equivalent documented setup paths, but they can require
  their normal mobile toolchains: JDK/Android SDK/NDK for Android, and
  macOS/Xcode/signing prerequisites for iOS.

## Release builds

- The package workflow runs only when manually dispatched or when a version tag
  matching `v*` is pushed.
- Release builds are responsible for platform packages, including Linux, macOS,
  Windows, Android, and iOS.
- iOS release work requires an explicit Xcode/signing/provisioning path and
  should not be added as an automatic per-push build.
- Use `/home/vega/Coding/GameDev/how-to-multi-backend-rendering` as the local
  iOS scaffold reference. Its working `ios-sim` CMake preset uses the Xcode
  generator, `CMAKE_SYSTEM_NAME=iOS`, simulator sysroot, arm64 simulator arch,
  bundled SDL, and app bundle metadata.
- Use the full matrix when we are making or validating a real release build.

## Gubsy dependency

Gubsy is a library/tool dependency. It should be validated locally during normal
development and pulled into Splonks releases through the Splonks release
pipeline.
