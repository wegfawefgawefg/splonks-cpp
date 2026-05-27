#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

version="${SPLONKS_RELEASE_VERSION:-${SPLONKS_ANDROID_VERSION_NAME:-0.1.0}}"
run_upload="${SPLONKS_PLAY_UPLOAD:-0}"

usage() {
    cat >&2 <<EOF
Usage: $0

Runs the Android Play release handoff from one entry point: preflight the real
upload keystore, build and verify the signed release AAB, optionally upload to
Google Play, then bundle the evidence.

Environment:
  SPLONKS_RELEASE_VERSION             Release version, default: ${version}
  SPLONKS_ANDROID_VERSION_CODE        Android versionCode for this release.
  SPLONKS_ANDROID_KEYSTORE            Real upload keystore path.
  SPLONKS_ANDROID_KEYSTORE_PASSWORD   Upload keystore password.
  SPLONKS_ANDROID_KEYSTORE_TYPE       Keystore type, default: jks.
  SPLONKS_ANDROID_KEYSTORE_PURPOSE    Must be upload.
  SPLONKS_ANDROID_KEY_ALIAS           Upload key alias.
  SPLONKS_ANDROID_KEY_PASSWORD        Upload key password.
  SPLONKS_PLAY_SERVICE_ACCOUNT_JSON   Google Play service account JSON.
  SPLONKS_ANDROID_PACKAGE_NAME        Package name, default: ${APP_ID}
  SPLONKS_PLAY_TRACK                  Play track, default: internal.
  SPLONKS_PLAY_RELEASE_STATUS         draft|completed|halted|inProgress.
  SPLONKS_PLAY_UPLOAD                 Set to 1 to run the Play upload helper.
  SPLONKS_PLAY_VALIDATE_ONLY          Set to 1 for Fastlane validate-only mode.

Upload mode intentionally defaults off. Set SPLONKS_PLAY_UPLOAD=1 only for a
real release candidate that should be sent to Google Play.
EOF
}

case "${1:-}" in
    "")
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

cd "${REPO_ROOT}"

echo "[android-play-handoff] release_version=${version}"
echo "[android-play-handoff] revision=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"

SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh android-release
SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh android-release

if [[ "${run_upload}" == "1" ]]; then
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh android-play
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh android-play-upload
else
    echo "[android-play-handoff] skipped Google Play upload; set SPLONKS_PLAY_UPLOAD=1 for Play upload validation"
fi

./scripts/bundle_validation_evidence.sh --include-artifacts android-play

echo "[android-play-handoff] Android Play handoff complete"
