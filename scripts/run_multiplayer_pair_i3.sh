#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

bash_command=${BASH:-/usr/bin/env bash}
script_path="${script_dir}/run_multiplayer_pair_i3.sh"
preset="${SPLONKS_PRESET:-release}"
binary_dir="build"
if [ "${preset}" = "dev" ]; then
    binary_dir="build-debug"
fi
binary="${repo_root}/${binary_dir}/splonks-cpp"
workspace="${SPLONKS_I3_WORKSPACE:-2}"
window_title="${SPLONKS_I3_WINDOW_TITLE:-^Splonks$}"
net_host="${SPLONKS_NET_HOST:-127.0.0.1}"
net_port="${SPLONKS_NET_PORT:-39000}"
host_ctl_port="${SPLONKS_HOST_CTL_PORT:-41000}"
join_ctl_port="${SPLONKS_JOIN_CTL_PORT:-41001}"

if [ "${1:-}" = "--child" ]; then
    role="${2:-offline}"
    cd "${repo_root}"
    case "${role}" in
        host)
            exec "${binary}" --multiplayer-host "${net_port}" --debug-control-port "${host_ctl_port}"
            ;;
        join)
            exec "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join_ctl_port}"
            ;;
        *)
            exec "${binary}"
            ;;
    esac
fi

"${script_dir}/build.sh"

if ! command -v i3-msg >/dev/null 2>&1; then
    echo "i3-msg not found; launching two Splonks instances without workspace placement." >&2
    (cd "${repo_root}" && "${binary}" --multiplayer-host "${net_port}" --debug-control-port "${host_ctl_port}") &
    sleep 0.4
    (cd "${repo_root}" && "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join_ctl_port}") &
    wait
    exit 0
fi

target_output="${SPLONKS_I3_OUTPUT:-DisplayPort-0}"

i3-msg "workspace number ${workspace}" >/dev/null
if [ -n "${target_output}" ]; then
    i3-msg "move workspace to output ${target_output}" >/dev/null
fi
i3-msg "layout splitv" >/dev/null

i3-msg "exec --no-startup-id ${bash_command} ${script_path} --child host" >/dev/null
sleep 0.4
i3-msg "exec --no-startup-id ${bash_command} ${script_path} --child join" >/dev/null

sleep "${SPLONKS_I3_SETTLE_SECONDS:-1.2}"
i3-msg "workspace number ${workspace}" >/dev/null
i3-msg "[title=\"${window_title}\"] floating disable" >/dev/null || true
i3-msg "[title=\"${window_title}\"] focus" >/dev/null || true
i3-msg "layout splitv" >/dev/null
