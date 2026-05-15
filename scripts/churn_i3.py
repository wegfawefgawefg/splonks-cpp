#!/usr/bin/env python3
"""i3 placement helpers for Splonks churn scripts."""

from __future__ import annotations

import json
import subprocess
import time
from typing import Any


def i3_available() -> bool:
    return (
        subprocess.run(["which", "i3-msg"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
        == 0
    )


def i3_msg(*args: str) -> None:
    subprocess.run(["i3-msg", *args], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def i3_arrange(workspace: str, output: str, cols: int, rows: int) -> int:
    if not i3_available():
        return 0
    i3_msg(f"workspace number {workspace}")
    if output:
        i3_msg(f"move workspace to output {output}")
    tree = subprocess.check_output(["i3-msg", "-t", "get_tree"], text=True)
    outputs = subprocess.check_output(["i3-msg", "-t", "get_outputs"], text=True)
    win_ids = find_splonks_window_ids(json.loads(tree), workspace)[-(cols * rows):]
    rect = choose_output_rect(json.loads(outputs), output)
    if not win_ids:
        return 0
    cell_w = max(240, int(rect["width"] // cols))
    cell_h = max(180, int(rect["height"] // rows))
    for index, window_id in enumerate(win_ids):
        col = index % cols
        row = index // cols
        x = int(rect["x"] + col * cell_w)
        y = int(rect["y"] + row * cell_h)
        i3_msg(f"[con_id={window_id}] floating enable")
        i3_msg(f"[con_id={window_id}] resize set {cell_w} {cell_h}")
        i3_msg(f"[con_id={window_id}] move position {x} {y}")
    return len(win_ids)


def i3_wait_and_arrange(
    workspace: str,
    output: str,
    cols: int,
    rows: int,
    expected_windows: int,
    timeout_s: float,
) -> int:
    if not i3_available():
        return 0
    deadline = time.monotonic() + timeout_s
    arranged = 0
    while True:
        arranged = i3_arrange(workspace, output, cols, rows)
        if arranged >= expected_windows or time.monotonic() >= deadline:
            return arranged
        time.sleep(0.1)


def choose_output_rect(outputs: list[dict[str, Any]], target: str) -> dict[str, int]:
    active = [output for output in outputs if output.get("active")]
    chosen = next((output for output in active if output.get("name") == target), None)
    if chosen is None and active:
        chosen = active[0]
    if chosen is None:
        return {"x": 0, "y": 0, "width": 1920, "height": 1080}
    rect = chosen.get("rect", {})
    return {
        key: int(rect.get(key, fallback))
        for key, fallback in (("x", 0), ("y", 0), ("width", 1920), ("height", 1080))
    }


def find_splonks_window_ids(tree: dict[str, Any], workspace: str) -> list[int]:
    ids: list[int] = []

    def walk(node: dict[str, Any], in_workspace: bool = False) -> None:
        node_type = node.get("type")
        node_name = str(node.get("name", ""))
        now_in_workspace = in_workspace or (node_type == "workspace" and node_name == workspace)
        if now_in_workspace and node.get("window"):
            title = (node.get("window_properties") or {}).get("title") or node_name
            if title == "Splonks":
                ids.append(int(node["id"]))
        for child in node.get("nodes", []) + node.get("floating_nodes", []):
            walk(child, now_in_workspace)

    walk(tree)
    return ids
