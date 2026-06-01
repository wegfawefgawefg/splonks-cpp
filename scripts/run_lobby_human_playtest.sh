#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"

server_port="${ROOM_SERVER_PORT:-8788}"
server_bind="${ROOM_SERVER_BIND:-127.0.0.1}"
server_url="${GUB_ROOM_SERVER_URL:-http://127.0.0.1:${server_port}}"
server_log="${repo_root}/logs/gubsy_roomd_lobby_playtest.log"
roomd_binary="${gubsy_root}/build/gubsy-roomd"
preset="${SPLONKS_PRESET:-release}"
binary_dir="build"
if [ "${preset}" = "dev" ]; then
    binary_dir="build-debug"
fi
binary="${repo_root}/${binary_dir}/splonks-cpp"

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*)
        binary="${binary}.exe"
        ;;
esac

usage() {
    printf '%s\n' \
        "Usage: scripts/run_lobby_human_playtest.sh [--no-build] [--no-roomd] [--host-only|--client-only] [--init-verdict] [--fill-verdict]" \
        "" \
        "Starts local gubsy-roomd plus two Splonks windows for the lobby" \
        "human playtest checklist." \
        "" \
        "Environment:" \
        "  ROOM_SERVER_PORT       Local roomd port when GUB_ROOM_SERVER_URL is unset (default: 8788)." \
        "  ROOM_SERVER_BIND       Address roomd binds when started by this script (default: 127.0.0.1)." \
        "  GUB_ROOM_SERVER_URL    Room server URL passed to both Splonks windows." \
        "  SPLONKS_PRESET         release or dev (default: release)." \
        "  SPLONKS_I3_WORKSPACE   i3 workspace for window placement (default: 2)." \
        "  SPLONKS_I3_OUTPUT      Optional i3 output for the workspace." \
        "  SDL_VIDEODRIVER        Defaults to x11 when DISPLAY is set." \
        "" \
        "Options:" \
        "  --host-only            Launch only the host Splonks window." \
        "  --client-only          Launch only the client Splonks window; implies --no-roomd." \
        "  --init-verdict         Create logs/lobby_human_playtest_verdict.json if missing." \
        "  --fill-verdict         Prompt for the verdict after both windows close, then audit it." \
        "" \
        "Checklist: docs/lobby_human_playtest_checklist.md" \
        "Verdict: logs/lobby_human_playtest_verdict.json"
}

build=1
start_roomd=1
start_roomd_set=0
init_verdict=0
fill_verdict=0
launch_mode="both"
while (($#)); do
    case "$1" in
        --no-build)
            build=0
            shift
            ;;
        --no-roomd)
            start_roomd=0
            start_roomd_set=1
            shift
            ;;
        --host-only)
            launch_mode="host"
            shift
            ;;
        --client-only)
            launch_mode="client"
            shift
            ;;
        --init-verdict)
            init_verdict=1
            shift
            ;;
        --fill-verdict)
            fill_verdict=1
            init_verdict=1
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

if [[ "${launch_mode}" == "client" && "${start_roomd_set}" == "0" ]]; then
    start_roomd=0
fi

init_verdict_file() {
    mkdir -p "${repo_root}/logs"
    verdict_path="${repo_root}/logs/lobby_human_playtest_verdict.json"
    if [[ -e "${verdict_path}" ]]; then
        printf 'Verdict JSON already exists: %s\n' "${verdict_path}"
    else
        cp "${repo_root}/docs/lobby_human_playtest_verdict_template.json" "${verdict_path}"
        printf 'Initialized verdict JSON: %s\n' "${verdict_path}"
    fi
}

if ((init_verdict != 0)); then
    init_verdict_file
fi

finish_verdict() {
    if ((fill_verdict == 0)); then
        return 0
    fi
    "${script_dir}/fill_lobby_human_playtest_verdict.py"
    "${script_dir}/summarize_lobby_human_playtest.py"
}

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

if ((build != 0)); then
    "${script_dir}/build.sh"
    if [ ! -x "${roomd_binary}" ] && ((start_roomd != 0)); then
        (cd "${gubsy_root}" && ./scripts/build.sh)
    fi
fi

if [[ ! -x "${binary}" ]]; then
    echo "Expected Splonks executable not found: ${binary}" >&2
    exit 1
fi
if ((start_roomd != 0)) && [[ ! -x "${roomd_binary}" ]]; then
    echo "Expected gubsy-roomd executable not found: ${roomd_binary}" >&2
    exit 1
fi

mkdir -p "${repo_root}/logs"
roomd_pid=""
# shellcheck disable=SC2317 # Called by the EXIT trap.
cleanup() {
    if [[ -n "${roomd_pid}" ]]; then
        kill "${roomd_pid}" >/dev/null 2>&1 || true
        wait "${roomd_pid}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

if ((start_roomd != 0)); then
    "${roomd_binary}" "--host=${server_bind}" "--port=${server_port}" >"${server_log}" 2>&1 &
    roomd_pid=$!
    for _ in $(seq 1 50); do
        if curl -fsS "${server_url}/rooms" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
fi

printf 'Lobby human playtest\n'
printf 'Room server: %s\n' "${server_url}"
if [[ -n "${roomd_pid}" ]]; then
    printf 'gubsy-roomd pid: %s\n' "${roomd_pid}"
    printf 'gubsy-roomd bind: %s:%s\n' "${server_bind}" "${server_port}"
    printf 'gubsy-roomd log: %s\n' "${server_log}"
fi
printf 'Checklist: docs/lobby_human_playtest_checklist.md\n\n'
printf 'After the playtest, fill and verify the verdict with:\n'
printf '  scripts/fill_lobby_human_playtest_verdict.py\n'
printf '  scripts/summarize_lobby_human_playtest.py\n\n'
printf 'Manual checks:\n'
printf '  1. Host window: Host Game -> Host Public.\n'
printf '  2. Client window: Join Game -> Browse Servers -> select host room.\n'
printf '  3. Host starts; client waits, then sees Play and enters gameplay/loading.\n'
printf '  4. Repeat Join By IP failure and success.\n'
printf '  5. Confirm lobby/in-game alerts.\n\n'

launch_child() {
    local role="$1"
    local ctl_port="$2"
    cd "${repo_root}"
    GUB_ROOM_SERVER_URL="${server_url}" "${binary}" \
        --debug-control-port "${ctl_port}" \
        --project-root "${repo_root}" &
    printf '%s pid: %s debug-control-port: %s\n' "${role}" "$!" "${ctl_port}"
}

launch_selected_children() {
    case "${launch_mode}" in
        both)
            launch_child "host-window" 41210
            sleep 0.4
            launch_child "client-window" 41211
            ;;
        host)
            launch_child "host-window" 41210
            ;;
        client)
            launch_child "client-window" 41211
            ;;
        *)
            printf 'unknown launch mode: %s\n' "${launch_mode}" >&2
            exit 2
            ;;
    esac
}

if ! command -v i3-msg >/dev/null 2>&1; then
    launch_selected_children
    playtest_status=0
    wait || playtest_status=$?
    finish_verdict
    exit "${playtest_status}"
fi

workspace="${SPLONKS_I3_WORKSPACE:-2}"
target_output="${SPLONKS_I3_OUTPUT:-}"
window_title="${SPLONKS_I3_WINDOW_TITLE:-^Splonks$}"
i3-msg "workspace number ${workspace}" >/dev/null
if [ -n "${target_output}" ]; then
    i3-msg "move workspace to output ${target_output}" >/dev/null
fi
i3-msg "layout splitv" >/dev/null

launch_selected_children
sleep "${SPLONKS_I3_SETTLE_SECONDS:-1.2}"
i3-msg "workspace number ${workspace}" >/dev/null
i3-msg "[title=\"${window_title}\"] floating disable" >/dev/null || true
i3-msg "[title=\"${window_title}\"] focus" >/dev/null || true
i3-msg "layout splitv" >/dev/null

playtest_status=0
wait || playtest_status=$?
finish_verdict
exit "${playtest_status}"
