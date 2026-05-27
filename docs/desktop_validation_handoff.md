# Desktop Validation Handoff

Use this when handing Splonks to a macOS or Windows developer for real-machine
validation. Linux has already been validated locally; macOS and Windows still
need these logs from actual target machines.

Generate the copy/paste instructions from the current commit:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh macos
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/print_validation_handoff.sh windows
```

The generated handoff pins validators to the exact Git commit being audited.
That matters because `validation_status.sh` rejects logs, manifests, and
bundles from a different revision unless the receiver explicitly sets
`SPLONKS_VALIDATION_REVISION`.

The goal is to prove that a new developer can clone, run one setup/build path,
and launch the game without understanding Gubsy internals or waiting on GitHub
Actions.

## macOS Developer Validation

Run on a real macOS machine:

```bash
xcode-select --install
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
git checkout <handoff-git-revision>
./scripts/bootstrap_dev.sh --run
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

Then bundle the validation evidence:

```bash
./scripts/bundle_validation_evidence.sh --include-artifacts macos
```

Send back the generated `dist/validation-bundles/splonks-validation-macos-*.tar.gz`.
The bundle helper only includes validation logs for the handoff commit and
release version, so rerun the `validate_platform.sh` commands after checking
out the pinned revision if bundling reports no matching logs.
On the receiving machine, import it with
`./scripts/import_validation_evidence.sh path/to/splonks-validation-macos-*.tar.gz`,
then run `validation_status.sh` with the same release version and handoff
revision printed by `print_validation_handoff.sh`.

Developer ID distribution still needs the signing/notarization path from
[release_distribution.md](release_distribution.md).
When Developer ID credentials are available, validate that path with:

```bash
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/validate_platform.sh macos-notarized
```

That validator signs, notarizes, staples, extracts the final zip, applies a
quarantine attribute, runs Gatekeeper assessment, and launches the smoke from
the quarantined extracted app.

## Windows Developer Validation

Run in the MSYS2 UCRT64 terminal, not PowerShell or cmd.exe:

```bash
pacman -Syu
git clone git@github.com:wegfawefgawefg/splonks-cpp.git
cd splonks-cpp
git checkout <handoff-git-revision>
./scripts/bootstrap_dev.sh --run
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

Then bundle the validation evidence:

```bash
./scripts/bundle_validation_evidence.sh --include-artifacts windows
```

Send back the generated `dist/validation-bundles/splonks-validation-windows-*.tar.gz`.
The bundle helper only includes validation logs for the handoff commit and
release version, so rerun the `validate_platform.sh` commands after checking
out the pinned revision if bundling reports no matching logs.
On the receiving machine, import it with
`./scripts/import_validation_evidence.sh path/to/splonks-validation-windows-*.tar.gz`,
then run `validation_status.sh` with the same release version and handoff
revision printed by `print_validation_handoff.sh`.

## Passing Result

A platform passes when:

- Setup completed from a fresh clone.
- `validate_platform.sh dev` passed.
- The interactive dev build launched.
- `validate_platform.sh release` passed.
- The packaged release artifact launched from its packaged/extracted form.
- The evidence log and any warnings were sent back to the team.
