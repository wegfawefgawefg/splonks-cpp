#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
run_upload="${SPLONKS_IOS_UPLOAD:-0}"
require_complete="${SPLONKS_IOS_REQUIRE_COMPLETE:-0}"
bundle_label="ios-partial"

usage() {
    cat >&2 <<EOF
Usage: $0

Runs the iOS external validation handoff from one entry point on macOS/Xcode:
simulator smoke, optional signed archive/export, optional physical device
install/launch, optional App Store Connect/TestFlight upload, then evidence
bundling.

Environment:
  SPLONKS_RELEASE_VERSION     Release version, default: ${version}
  SPLONKS_IOS_BUNDLE_ID       Bundle id, default: dev.splonks.game
  SPLONKS_IOS_DEVELOPMENT_TEAM
                              Enables signed archive/export validation.
  SPLONKS_IOS_DEVICE_ID       Enables physical device install/launch validation.
  SPLONKS_IOS_UPLOAD          Set to 1 to run App Store Connect upload validation.
  SPLONKS_IOS_REQUIRE_COMPLETE
                              Set to 1 for final release evidence; requires
                              signing, device, and upload inputs before bundling.

Upload mode intentionally defaults off. Set SPLONKS_IOS_UPLOAD=1 only for a
real release candidate that should be sent to App Store Connect/TestFlight.
The default mode is useful for simulator-only or partial validation. Final iOS
release evidence must use SPLONKS_IOS_REQUIRE_COMPLETE=1.
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

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "validate_ios_handoff.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

cd "${repo_root}"

echo "[ios-handoff] release_version=${version}"
echo "[ios-handoff] revision=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"

if [[ "${require_complete}" == "1" ]]; then
    bundle_label="ios"
    complete_failures=0
    require_complete_env() {
        local name="$1"
        if [[ -n "${!name:-}" ]]; then
            return 0
        fi
        echo "[ios-handoff] complete evidence requires ${name}" >&2
        complete_failures=$((complete_failures + 1))
    }

    require_complete_env SPLONKS_IOS_DEVELOPMENT_TEAM
    require_complete_env SPLONKS_IOS_DEVICE_ID
    if [[ "${run_upload}" != "1" ]]; then
        echo "[ios-handoff] complete evidence requires SPLONKS_IOS_UPLOAD=1" >&2
        complete_failures=$((complete_failures + 1))
    fi
    if [[ "${complete_failures}" -ne 0 ]]; then
        exit 1
    fi
fi

SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh ios-sim
SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh ios-sim

if [[ -n "${SPLONKS_IOS_DEVELOPMENT_TEAM:-}" ]]; then
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh ios-release
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh ios-release
else
    echo "[ios-handoff] skipped signed archive/export; SPLONKS_IOS_DEVELOPMENT_TEAM is not set"
fi

if [[ -n "${SPLONKS_IOS_DEVICE_ID:-}" ]]; then
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh ios-device
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh ios-device
else
    echo "[ios-handoff] skipped physical device install; SPLONKS_IOS_DEVICE_ID is not set"
fi

if [[ "${run_upload}" == "1" ]]; then
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/release_credentials_preflight.sh ios-upload
    SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh ios-upload
else
    echo "[ios-handoff] skipped App Store Connect upload; set SPLONKS_IOS_UPLOAD=1 for release upload validation"
fi

./scripts/bundle_validation_evidence.sh --include-artifacts "${bundle_label}"

echo "[ios-handoff] iOS handoff complete"
