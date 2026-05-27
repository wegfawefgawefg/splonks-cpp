# Desktop Validation Handoff

Use this when handing Splonks to a macOS or Windows developer for real-machine
validation. Linux has already been validated locally; macOS and Windows still
need these logs from actual target machines.

The goal is to prove that a new developer can clone, run one setup/build path,
and launch the game without understanding Gubsy internals or waiting on GitHub
Actions.

## macOS Developer Validation

Run on a real macOS machine:

```bash
xcode-select --install
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/bootstrap_dev.sh
SPLONKS_PRESET=dev ./scripts/run.sh
./scripts/validate_platform.sh dev
```

Record:

- macOS version.
- The generated `dist/validation/macos-dev-*.log`.
- Whether the interactive game window opened.
- Any SDL, audio, controller, or file permission warnings.

The generated validation log records the Homebrew, Git, CMake, Ninja,
compiler, Xcode, and command line tools details when those tools are present.
Only record them separately if the log is missing a field or the machine has an
unusual setup.

## macOS Release Validation

Run after developer validation:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh release
```

Record:

- `dist/validation/macos-release-*.log`.
- `dist/releases/splonks-0.1.0-macos-universal.zip`.
- `dist/releases/splonks-0.1.0-macos-universal.zip.sha256`.
- Whether the packaged `Splonks.app` launches locally.

Developer ID distribution still needs the signing/notarization path from
[release_distribution.md](release_distribution.md).

## Windows Developer Validation

Run in the MSYS2 UCRT64 terminal, not PowerShell or cmd.exe:

```bash
pacman -Syu
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
./scripts/bootstrap_dev.sh
SPLONKS_PRESET=dev ./scripts/run.sh
./scripts/validate_platform.sh dev
```

If `pacman -Syu` asks to close the terminal, reopen MSYS2 UCRT64 and continue
from `./scripts/setup_windows_msys2.sh`.

Record:

- Windows version.
- The generated `dist/validation/windows-dev-*.log`.
- Whether the interactive game window opened.
- Any SDL, audio, controller, or file permission warnings.

The generated validation log records `MSYSTEM`, Git, CMake, Ninja, pkg-config,
GCC/Clang, pacman, and `PATH` details when those tools are present. Only record
them separately if the log is missing a field or the machine has an unusual
setup.

## Windows Release Validation

Run after developer validation in MSYS2 UCRT64:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh release
```

Record:

- `dist/validation/windows-release-*.log`.
- `dist/releases/splonks-0.1.0-windows-x86_64.zip`.
- `dist/releases/splonks-0.1.0-windows-x86_64.zip.sha256`.
- Whether `run-splonks.bat` launches from an extracted copy of the zip.

## Passing Result

A platform passes when:

- Setup completed from a fresh clone.
- `validate_platform.sh dev` passed.
- The interactive dev build launched.
- `validate_platform.sh release` passed.
- The packaged release artifact launched from its packaged/extracted form.
- The evidence log and any warnings were sent back to the team.
