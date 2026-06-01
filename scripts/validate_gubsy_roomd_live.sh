#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
splonks_binary="${repo_root}/build/splonks-cpp"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"
roomd_binary="${gubsy_root}/build/gubsy-roomd"
server_port="${ROOM_SERVER_PORT:-8790}"
server_bind="127.0.0.1"
server_host="127.0.0.1"
server_log="${repo_root}/logs/gubsy_roomd_live_smoke.log"

usage() {
    printf '%s\n' \
        "Usage: scripts/validate_gubsy_roomd_live.sh [--lan-interface]" \
        "" \
        "Runs the Splonks real-roomd host/browser-join smoke." \
        "" \
        "Options:" \
        "  --lan-interface    Bind roomd to 0.0.0.0 and reach it through this machine's LAN IPv4." \
        "" \
        "Environment:" \
        "  ROOM_SERVER_PORT   Roomd port (default: 8790)." \
        "  ROOM_SERVER_HOST   Override the URL host used with --lan-interface."
}

while (($#)); do
    case "$1" in
        --lan-interface)
            server_bind="0.0.0.0"
            server_host="${ROOM_SERVER_HOST:-}"
            if [[ -z "${server_host}" ]]; then
                server_host="$(hostname -I | awk '{for (i = 1; i <= NF; ++i) if ($i !~ /^127\\./) {print $i; exit}}')"
            fi
            if [[ -z "${server_host}" ]]; then
                echo "Could not determine a LAN IPv4 address; set ROOM_SERVER_HOST." >&2
                exit 2
            fi
            server_log="${repo_root}/logs/gubsy_roomd_live_lan_smoke.log"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

server_url="http://${server_host}:${server_port}"

if [ ! -x "${splonks_binary}" ]; then
    "${script_dir}/build.sh"
fi

if [ ! -x "${roomd_binary}" ]; then
    (cd "${gubsy_root}" && ./scripts/build.sh)
fi

mkdir -p "${repo_root}/logs"
"${roomd_binary}" "--host=${server_bind}" "--port=${server_port}" >"${server_log}" 2>&1 &
server_pid=$!

cleanup() {
    kill "${server_pid}" >/dev/null 2>&1 || true
    wait "${server_pid}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    if NO_PROXY="${server_host},${NO_PROXY:-}" no_proxy="${server_host},${no_proxy:-}" \
        curl -fsS "${server_url}/rooms" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

GUB_ROOM_SERVER_URL="${server_url}" "${splonks_binary}" --check-gubsy-shell-real-roomd-smoke
