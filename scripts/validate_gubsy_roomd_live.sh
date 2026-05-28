#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
splonks_binary="${repo_root}/build/splonks-cpp"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"
roomd_binary="${gubsy_root}/build/gubsy-roomd"
server_port="${ROOM_SERVER_PORT:-8790}"
server_url="http://127.0.0.1:${server_port}"
server_log="${repo_root}/logs/gubsy_roomd_live_smoke.log"

if [ ! -x "${splonks_binary}" ]; then
    "${script_dir}/build.sh"
fi

if [ ! -x "${roomd_binary}" ]; then
    (cd "${gubsy_root}" && ./scripts/build.sh)
fi

mkdir -p "${repo_root}/logs"
"${roomd_binary}" "--port=${server_port}" >"${server_log}" 2>&1 &
server_pid=$!

cleanup() {
    kill "${server_pid}" >/dev/null 2>&1 || true
    wait "${server_pid}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    if curl -fsS "${server_url}/rooms" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

GUB_ROOM_SERVER_URL="${server_url}" "${splonks_binary}" --check-gubsy-shell-real-roomd-smoke
