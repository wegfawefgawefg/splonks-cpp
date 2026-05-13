#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

bash_command=${BASH:-/usr/bin/env bash}
script_path="${script_dir}/run_multiplayer_quad_i3.sh"
preset="${SPLONKS_PRESET:-release}"
binary_dir="build"
if [ "${preset}" = "dev" ]; then
    binary_dir="build-debug"
fi
binary="${repo_root}/${binary_dir}/splonks-cpp"
workspace="${SPLONKS_I3_WORKSPACE:-2}"
window_title="${SPLONKS_I3_WINDOW_TITLE:-^Splonks$}"
target_output="${SPLONKS_I3_OUTPUT:-DisplayPort-0}"
net_host="${SPLONKS_NET_HOST:-127.0.0.1}"
net_port="${SPLONKS_NET_PORT:-39000}"
host_ctl_port="${SPLONKS_HOST_CTL_PORT:-41000}"
join1_ctl_port="${SPLONKS_JOIN1_CTL_PORT:-41001}"
join2_ctl_port="${SPLONKS_JOIN2_CTL_PORT:-41002}"
join3_ctl_port="${SPLONKS_JOIN3_CTL_PORT:-41003}"
quad_width="${SPLONKS_QUAD_WINDOW_WIDTH:-960}"
quad_height="${SPLONKS_QUAD_WINDOW_HEIGHT:-540}"

random_input_args=()
if [ "${SPLONKS_RANDOM_PRIMARY_INPUTS:-1}" != "0" ]; then
    random_input_args+=(--debug-random-primary-input)
fi

kill_existing_quad_processes() {
    pkill -f "${binary} --multiplayer-host ${net_port} --debug-control-port ${host_ctl_port}" >/dev/null 2>&1 || true
    pkill -f "${binary} --multiplayer-join ${net_host} ${net_port} --debug-control-port ${join1_ctl_port}" >/dev/null 2>&1 || true
    pkill -f "${binary} --multiplayer-join ${net_host} ${net_port} --debug-control-port ${join2_ctl_port}" >/dev/null 2>&1 || true
    pkill -f "${binary} --multiplayer-join ${net_host} ${net_port} --debug-control-port ${join3_ctl_port}" >/dev/null 2>&1 || true
}

run_child() {
    local role="${1:-offline}"
    cd "${repo_root}"
    case "${role}" in
        host)
            exec "${binary}" --multiplayer-host "${net_port}" --debug-control-port "${host_ctl_port}" "${random_input_args[@]}"
            ;;
        join1)
            exec "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join1_ctl_port}" "${random_input_args[@]}"
            ;;
        join2)
            exec "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join2_ctl_port}" "${random_input_args[@]}"
            ;;
        join3)
            exec "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join3_ctl_port}" "${random_input_args[@]}"
            ;;
        *)
            exec "${binary}" "${random_input_args[@]}"
            ;;
    esac
}

arrange_quad_windows() {
    local output_json tree_json output_rect window_ids
    output_json="$(i3-msg -t get_outputs)"
    tree_json="$(i3-msg -t get_tree)"
    output_rect="$(
        OUTPUT_JSON="${output_json}" TARGET_OUTPUT="${target_output}" python3 - <<'PY'
import json
import os

outputs = json.loads(os.environ["OUTPUT_JSON"])
target = os.environ.get("TARGET_OUTPUT", "")
active = [o for o in outputs if o.get("active")]
chosen = None
if target:
    chosen = next((o for o in active if o.get("name") == target), None)
if chosen is None and active:
    chosen = active[0]
if chosen is None:
    print("0 0 1920 1080")
else:
    rect = chosen.get("rect", {})
    print(f"{rect.get('x', 0)} {rect.get('y', 0)} {rect.get('width', 1920)} {rect.get('height', 1080)}")
PY
    )"
    window_ids="$(
        TREE_JSON="${tree_json}" WORKSPACE="${workspace}" TITLE_REGEX="${window_title}" python3 - <<'PY'
import json
import os
import re

tree = json.loads(os.environ["TREE_JSON"])
workspace = os.environ["WORKSPACE"]
title_re = re.compile(os.environ["TITLE_REGEX"])
ids = []

def walk(node, in_workspace=False):
    node_type = node.get("type")
    node_name = str(node.get("name", ""))
    now_in_workspace = in_workspace or (node_type == "workspace" and node_name == workspace)
    if now_in_workspace and node.get("window"):
        props = node.get("window_properties") or {}
        title = props.get("title") or node_name
        if title_re.search(title):
            ids.append(str(node["id"]))
    for child in node.get("nodes", []) + node.get("floating_nodes", []):
        walk(child, now_in_workspace)

walk(tree)
print(" ".join(ids[-4:]))
PY
    )"

    read -r output_x output_y _output_w _output_h <<<"${output_rect}"
    read -r -a ids <<<"${window_ids}"
    if [ "${#ids[@]}" -lt 4 ]; then
        echo "Expected 4 Splonks windows on workspace ${workspace}, found ${#ids[@]}." >&2
        return 1
    fi

    local positions=(
        "${output_x} ${output_y}"
        "$((output_x + quad_width)) ${output_y}"
        "$((output_x + quad_width)) $((output_y + quad_height))"
        "${output_x} $((output_y + quad_height))"
    )

    for index in 0 1 2 3; do
        read -r pos_x pos_y <<<"${positions[${index}]}"
        i3-msg "[con_id=${ids[${index}]}] floating enable" >/dev/null || true
        i3-msg "[con_id=${ids[${index}]}] resize set ${quad_width} ${quad_height}" >/dev/null || true
        i3-msg "[con_id=${ids[${index}]}] move position ${pos_x} ${pos_y}" >/dev/null || true
    done
}

if [ "${1:-}" = "--child" ]; then
    run_child "${2:-offline}"
fi

"${script_dir}/build.sh"

rm -f "${repo_root}"/logs/network_events_host_p*.log \
      "${repo_root}"/logs/network_events_peer_p*.log

if [ "${SPLONKS_KILL_EXISTING_QUAD:-1}" != "0" ]; then
    kill_existing_quad_processes
fi

if ! command -v i3-msg >/dev/null 2>&1; then
    echo "i3-msg not found; launching four Splonks instances without workspace placement." >&2
    (cd "${repo_root}" && "${binary}" --multiplayer-host "${net_port}" --debug-control-port "${host_ctl_port}" "${random_input_args[@]}") &
    sleep 0.4
    (cd "${repo_root}" && "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join1_ctl_port}" "${random_input_args[@]}") &
    sleep 0.4
    (cd "${repo_root}" && "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join2_ctl_port}" "${random_input_args[@]}") &
    sleep 0.4
    (cd "${repo_root}" && "${binary}" --multiplayer-join "${net_host}" "${net_port}" --debug-control-port "${join3_ctl_port}" "${random_input_args[@]}") &
    wait
    exit 0
fi

if [ "${SPLONKS_KILL_EXISTING_QUAD:-1}" != "0" ]; then
    i3-msg "[workspace=\"${workspace}\" title=\"${window_title}\"] kill" >/dev/null || true
    sleep 0.25
    kill_existing_quad_processes
fi

i3-msg "workspace number ${workspace}" >/dev/null
if [ -n "${target_output}" ]; then
    i3-msg "move workspace to output ${target_output}" >/dev/null
fi

for role in host join1 join2 join3; do
    i3-msg "exec --no-startup-id ${bash_command} ${script_path} --child ${role}" >/dev/null
    sleep 0.35
done

sleep "${SPLONKS_I3_SETTLE_SECONDS:-1.8}"
i3-msg "workspace number ${workspace}" >/dev/null
arrange_quad_windows || true
