#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
splonks_binary="${repo_root}/build/splonks-cpp"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"
roomd_binary="${gubsy_root}/build/gubsy-roomd"
server_port="${ROOM_SERVER_PORT:-8790}"
punch_port="${ROOM_PUNCH_PORT:-$((server_port + 1))}"
relay_port="${ROOM_RELAY_PORT:-$((server_port + 2))}"
server_bind="${ROOM_SERVER_BIND:-127.0.0.1}"
server_host="${ROOM_SERVER_HOST:-127.0.0.1}"
server_url="http://${server_host}:${server_port}"
max_host_frames="${REALNET_RELAY_HOST_FRAMES:-2400}"
max_client_frames="${REALNET_RELAY_CLIENT_FRAMES:-2400}"
logs_dir="${repo_root}/logs"
roomd_log="${logs_dir}/realnet_relay_roomd.log"
host_log="${logs_dir}/realnet_relay_host.log"
client_log="${logs_dir}/realnet_relay_client.log"

usage() {
    printf '%s\n' \
        "Usage: scripts/validate_realnet_relay_live.sh" \
        "" \
        "Starts local gubsy-roomd with relay enabled, then validates Splonks host/client" \
        "connection through a forced Realnet relay candidate." \
        "" \
        "Environment:" \
        "  ROOM_SERVER_PORT             HTTP port (default: 8790)." \
        "  ROOM_PUNCH_PORT              Punch UDP port (default: HTTP + 1)." \
        "  ROOM_RELAY_PORT              Relay UDP port (default: HTTP + 2)." \
        "  ROOM_SERVER_BIND             roomd bind host (default: 127.0.0.1)." \
        "  ROOM_SERVER_HOST             URL host clients use (default: 127.0.0.1)." \
        "  REALNET_RELAY_HOST_FRAMES    Host smoke frame budget (default: 2400)." \
        "  REALNET_RELAY_CLIENT_FRAMES  Client smoke frame budget (default: 2400)."
}

if (($# > 0)); then
    case "$1" in
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
fi

if [ ! -x "${splonks_binary}" ]; then
    "${script_dir}/build.sh"
fi

if [ ! -x "${roomd_binary}" ]; then
    (cd "${gubsy_root}" && ./scripts/build.sh)
fi

mkdir -p "${logs_dir}"
: >"${roomd_log}"
: >"${host_log}"
: >"${client_log}"

roomd_pid=""
host_pid=""

cleanup() {
    if [[ -n "${host_pid}" ]]; then
        kill "${host_pid}" >/dev/null 2>&1 || true
        wait "${host_pid}" >/dev/null 2>&1 || true
    fi
    if [[ -n "${roomd_pid}" ]]; then
        kill "${roomd_pid}" >/dev/null 2>&1 || true
        wait "${roomd_pid}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

"${roomd_binary}" \
    "--host=${server_bind}" \
    "--port=${server_port}" \
    "--punch-port=${punch_port}" \
    "--relay" \
    "--relay-port=${relay_port}" \
    >"${roomd_log}" 2>&1 &
roomd_pid=$!

for _ in $(seq 1 50); do
    if NO_PROXY="${server_host},${NO_PROXY:-}" no_proxy="${server_host},${no_proxy:-}" \
        curl -fsS "${server_url}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

if ! NO_PROXY="${server_host},${NO_PROXY:-}" no_proxy="${server_host},${no_proxy:-}" \
    curl -fsS "${server_url}/health" >/dev/null; then
    echo "roomd did not become healthy at ${server_url}; see ${roomd_log}" >&2
    exit 1
fi

SPLONKS_REALNET_FORCE_RELAY=1 \
SPLONKS_REALNET_RELAY_PORT="${relay_port}" \
"${splonks_binary}" --check-gubsy-shell-realnet-lan-host \
    "${server_url}" "${max_host_frames}" >"${host_log}" 2>&1 &
host_pid=$!

room_code=""
for _ in $(seq 1 100); do
    if ! kill -0 "${host_pid}" >/dev/null 2>&1; then
        echo "host smoke exited before publishing a room; see ${host_log}" >&2
        wait "${host_pid}" || true
        exit 1
    fi
    room_code="$(sed -n 's/^REALNET_LAN_HOST_READY room_code=\([^ ]*\).*/\1/p' "${host_log}" | tail -n 1)"
    if [[ -n "${room_code}" ]]; then
        break
    fi
    sleep 0.1
done

if [[ -z "${room_code}" ]]; then
    echo "host smoke did not publish a room code; see ${host_log}" >&2
    exit 1
fi

SPLONKS_REALNET_FORCE_RELAY=1 \
SPLONKS_REALNET_RELAY_PORT="${relay_port}" \
"${splonks_binary}" --check-gubsy-shell-realnet-lan-client \
    "${server_url}" "${room_code}" "${max_client_frames}" >"${client_log}" 2>&1

wait "${host_pid}"
host_pid=""

printf 'Realnet forced-relay smoke passed.\n'
printf '  roomd: %s\n' "${roomd_log}"
printf '  host:  %s\n' "${host_log}"
printf '  client:%s\n' "${client_log}"
