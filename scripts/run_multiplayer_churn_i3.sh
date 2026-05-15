#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

preset="${SPLONKS_PRESET:-release}"
binary_dir="build"
if [ "${preset}" = "dev" ]; then
    binary_dir="build-debug"
fi

binary="${repo_root}/${binary_dir}/splonks-cpp"
net_port="${SPLONKS_NET_PORT:-39000}"
host_ctl_port="${SPLONKS_HOST_CTL_PORT:-41000}"
peer_ctl_base="${SPLONKS_PEER_CTL_BASE:-41001}"
max_peers="${SPLONKS_CHURN_MAX_PEERS:-7}"
mode="${SPLONKS_CHURN_MODE:-cohort}"
cohort_size="${SPLONKS_CHURN_COHORT_SIZE:-${max_peers}}"
play_window_s="${SPLONKS_CHURN_PLAY_WINDOW_S:-8}"
duration_s="${SPLONKS_CHURN_DURATION_S:-300}"
min_live_s="${SPLONKS_CHURN_MIN_LIVE_S:-1}"
max_live_s="${SPLONKS_CHURN_MAX_LIVE_S:-30}"
hard_drop_percent="${SPLONKS_CHURN_HARD_DROP_PERCENT:-35}"
sync_drop_percent="${SPLONKS_CHURN_SYNC_DROP_PERCENT:-0}"
rejoin_percent="${SPLONKS_CHURN_REJOIN_PERCENT:-50}"
input_chance="${SPLONKS_CHURN_INPUT_CHANCE:-0.18}"
workspace="${SPLONKS_I3_WORKSPACE:-2}"
target_output="${SPLONKS_I3_OUTPUT:-DisplayPort-0}"
i3_cols="${SPLONKS_CHURN_I3_COLS:-2}"
i3_rows="${SPLONKS_CHURN_I3_ROWS:-4}"
random_host_args=()
if [ "${SPLONKS_CHURN_RANDOM_HOST:-0}" != "0" ]; then
    random_host_args+=(--random-host-input)
fi

"${script_dir}/build.sh"

cd "${repo_root}"
exec python3 "${script_dir}/churn_lockstep_live.py" \
    --repo-root "${repo_root}" \
    --binary "${binary}" \
    --net-port "${net_port}" \
    --host-control-port "${host_ctl_port}" \
    --peer-control-base "${peer_ctl_base}" \
    --max-peers "${max_peers}" \
    --mode "${mode}" \
    --cohort-size "${cohort_size}" \
    --play-window-s "${play_window_s}" \
    --duration-s "${duration_s}" \
    --min-live-s "${min_live_s}" \
    --max-live-s "${max_live_s}" \
    --hard-drop-percent "${hard_drop_percent}" \
    --sync-drop-percent "${sync_drop_percent}" \
    --rejoin-percent "${rejoin_percent}" \
    --input-chance "${input_chance}" \
    "${random_host_args[@]}" \
    --i3-workspace "${workspace}" \
    --i3-output "${target_output}" \
    --i3-cols "${i3_cols}" \
    --i3-rows "${i3_rows}" \
    --kill-existing
