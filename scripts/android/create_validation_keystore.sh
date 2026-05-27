#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_java
require_cmd keytool

keystore_path="${1:-${REPO_ROOT}/dist/local/android-validation.jks}"
keystore_password="${SPLONKS_ANDROID_KEYSTORE_PASSWORD:-android}"
key_alias="${SPLONKS_ANDROID_KEY_ALIAS:-splonks}"
key_password="${SPLONKS_ANDROID_KEY_PASSWORD:-${keystore_password}}"
distinguished_name="${SPLONKS_ANDROID_KEY_DNAME:-CN=Splonks Local Validation,O=Splonks,C=US}"
keystore_purpose="validation"

mkdir -p "$(dirname "${keystore_path}")"

if [[ -e "${keystore_path}" ]]; then
    echo "[android-validation-keystore] reusing ${keystore_path}" >&2
else
    keytool -genkeypair \
        -storetype JKS \
        -keystore "${keystore_path}" \
        -storepass "${keystore_password}" \
        -keypass "${key_password}" \
        -alias "${key_alias}" \
        -keyalg RSA \
        -keysize 2048 \
        -validity 10000 \
        -dname "${distinguished_name}"

    echo "[android-validation-keystore] ${keystore_path}" >&2
fi

cat <<EOF
export SPLONKS_ANDROID_KEYSTORE="${keystore_path}"
export SPLONKS_ANDROID_KEYSTORE_PASSWORD="${keystore_password}"
export SPLONKS_ANDROID_KEYSTORE_TYPE="jks"
export SPLONKS_ANDROID_KEYSTORE_PURPOSE="${keystore_purpose}"
export SPLONKS_ANDROID_KEY_ALIAS="${key_alias}"
export SPLONKS_ANDROID_KEY_PASSWORD="${key_password}"
EOF
