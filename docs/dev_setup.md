# Developer Setup

This document is the onboarding contract for Splonks contributors. The goal is
that a developer can clone the repo, follow one platform section, build once,
and launch the game locally without waiting on GitHub Actions.

Gubsy is a library/tool dependency. Normal Splonks development should not
require manually packaging Gubsy.

## Linux

Target bar:

- Clone Splonks.
- Install documented native build packages.
- Run one setup/build command.
- Launch Splonks from the local checkout.

Status: needs a clean-clone verification pass and exact package list.

## macOS

Target bar:

- Clone Splonks.
- Install Xcode command line tools and documented Homebrew packages.
- Run one setup/build command.
- Launch Splonks from the local checkout.

Status: needs macOS verification and exact Homebrew package list.

## Windows

Target bar:

- Clone Splonks.
- Install the documented supported Windows toolchain.
- Run one setup/build command.
- Launch Splonks from the local checkout.

Initial supported path should be MSYS2/UCRT unless we add and validate a Visual
Studio path.

Status: needs Windows verification and exact MSYS2 package list.

## Android

Target bar:

- Install JDK, Android SDK, and Android NDK prerequisites.
- Run the Android setup/build scripts.
- Build, install, and launch an APK on an emulator or device.

Status: scaffold exists; runtime smoke on emulator/device still needs
validation.

## iOS

Target bar:

- Use macOS and Xcode.
- Mirror the proven local `how-to-multi-backend-rendering` `ios-sim` scaffold.
- Build and launch the iOS simulator app.
- Document device signing, provisioning, and TestFlight requirements.

Status: target documented; Splonks iOS scaffold is not implemented yet.
