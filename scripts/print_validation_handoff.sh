#!/usr/bin/env bash
set -euo pipefail

version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
target="${1:-all}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
current_revision="$(git -C "${repo_root}" rev-parse HEAD 2>/dev/null || echo unknown)"
current_branch="$(git -C "${repo_root}" branch --show-current 2>/dev/null || true)"
revision="${SPLONKS_VALIDATION_REVISION:-${current_revision}}"
short_revision="${revision:0:12}"
validation_ref="${SPLONKS_VALIDATION_REF:-${current_branch:-net-lockstep-experiment}}"

usage() {
    cat >&2 <<EOF
Usage: $0 [macos|windows|android-play|ios|all]

Prints a copy/paste validation handoff for external platform validators.

Environment:
  SPLONKS_RELEASE_VERSION   Release version used in artifact names, default: ${version}
  SPLONKS_VALIDATION_REF    Remote ref to fetch before checkout, default: ${validation_ref}
  SPLONKS_VALIDATION_REVISION
                            Commit to validate, default: current HEAD
EOF
}

header() {
    printf '\n## %s\n\n' "$1"
}

common_return() {
    local label="$1"
    cat <<EOF

Return:
- The generated dist/validation-bundles/splonks-validation-${label}-*.tar.gz
- Notes on whether the interactive app launched and any warnings seen

Receiver import:

    ./scripts/import_validation_evidence.sh path/to/splonks-validation-${label}-*.tar.gz
    SPLONKS_RELEASE_VERSION=${version} SPLONKS_VALIDATION_REVISION=${short_revision} ./scripts/validation_status.sh
EOF
}

checkout_commands() {
    cat <<EOF
    git clone git@github.com:wegfawefgawefg/splonks-cpp.git
    cd splonks-cpp
    git fetch --tags origin ${validation_ref}
    git checkout ${revision}
EOF
}

generated_for() {
    cat <<EOF
Generated for:
- release_version=${version}
- validation_ref=${validation_ref}
- validation_revision=${revision}
EOF
}

print_macos() {
    header "macOS Developer And Release Validation"
    generated_for
    cat <<EOF

Run on a real macOS machine with Xcode command line tools and Homebrew:

    xcode-select --install
EOF
    checkout_commands
    cat <<EOF
    ./scripts/bootstrap_dev.sh --run
    ./scripts/validate_platform.sh dev
    SPLONKS_RELEASE_VERSION=${version} ./scripts/validate_platform.sh release
    ./scripts/bundle_validation_evidence.sh --include-artifacts macos

If Developer ID credentials are available:

    export SPLONKS_RELEASE_VERSION=${version}
    export SPLONKS_MACOS_BUNDLE_ID=dev.splonks.game
    export SPLONKS_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
    export SPLONKS_NOTARYTOOL_PROFILE=splonks-notary
    ./scripts/release_credentials_preflight.sh macos-notarized
    ./scripts/validate_platform.sh macos-notarized
    ./scripts/bundle_validation_evidence.sh --include-artifacts macos-notarized
EOF
    common_return "macos"
}

print_windows() {
    header "Windows MSYS2/UCRT64 Developer And Release Validation"
    generated_for
    cat <<EOF

Run in the MSYS2 UCRT64 terminal, not PowerShell or cmd.exe:

    pacman -Syu

If pacman asks to close the terminal, reopen MSYS2 UCRT64 and continue:

EOF
    checkout_commands
    cat <<EOF
    ./scripts/bootstrap_dev.sh --run
    ./scripts/validate_platform.sh dev
    SPLONKS_RELEASE_VERSION=${version} ./scripts/validate_platform.sh release
    ./scripts/bundle_validation_evidence.sh --include-artifacts windows
EOF
    common_return "windows"
}

print_android_play() {
    header "Android Play Internal Testing Upload Validation"
    generated_for
    cat <<EOF

Run after the signed AAB has been built with the real upload key:

EOF
    checkout_commands
    cat <<EOF
    export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
    export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
    export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
    export SPLONKS_ANDROID_KEY_ALIAS=...
    export SPLONKS_ANDROID_KEY_PASSWORD=...
    export SPLONKS_ANDROID_VERSION_CODE=1
    export SPLONKS_ANDROID_VERSION_NAME=${version}
    ./scripts/release_credentials_preflight.sh android-release
    ./scripts/validate_platform.sh android-release

Then validate or upload to the internal Play track:

    gem install fastlane
    export SPLONKS_PLAY_SERVICE_ACCOUNT_JSON=/absolute/path/to/google-play-service-account.json
    export SPLONKS_ANDROID_PACKAGE_NAME=dev.splonks.game
    export SPLONKS_PLAY_TRACK=internal
    export SPLONKS_PLAY_RELEASE_STATUS=draft
    ./scripts/release_credentials_preflight.sh android-play
    ./scripts/validate_platform.sh android-play-upload
    ./scripts/bundle_validation_evidence.sh --include-artifacts android-play
EOF
    common_return "android-play"
}

print_ios() {
    header "iOS Simulator, Archive, And TestFlight Validation"
    generated_for
    cat <<EOF

Run on a real macOS machine with Xcode and an Apple Developer team:

EOF
    checkout_commands
    cat <<EOF
    export SPLONKS_RELEASE_VERSION=${version}
    export SPLONKS_IOS_BUNDLE_ID=dev.splonks.game
    ./scripts/validate_platform.sh ios-sim

Archive/export with signing:

    export SPLONKS_IOS_DEVELOPMENT_TEAM=TEAMID
    export SPLONKS_IOS_CODE_SIGN_IDENTITY="Apple Distribution"
    export SPLONKS_IOS_EXPORT_METHOD=app-store-connect
    ./scripts/release_credentials_preflight.sh ios-release
    ./scripts/validate_platform.sh ios-release

Validate/upload to App Store Connect/TestFlight:

    export SPLONKS_APP_STORE_API_KEY=ABC123DEFG
    export SPLONKS_APP_STORE_API_ISSUER=00000000-0000-0000-0000-000000000000
    export API_PRIVATE_KEYS_DIR=/absolute/path/to/appstoreconnect/private_keys
    ./scripts/release_credentials_preflight.sh ios-upload
    ./scripts/validate_platform.sh ios-upload
    ./scripts/bundle_validation_evidence.sh --include-artifacts ios
EOF
    common_return "ios"
}

case "${target}" in
    macos)
        print_macos
        ;;
    windows)
        print_windows
        ;;
    android-play)
        print_android_play
        ;;
    ios)
        print_ios
        ;;
    all)
        print_macos
        print_windows
        print_android_play
        print_ios
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac
