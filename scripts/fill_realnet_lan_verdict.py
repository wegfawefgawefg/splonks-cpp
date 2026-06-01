#!/usr/bin/env python3
"""Interactively fill the Realnet LAN validation verdict JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_TEMPLATE = "docs/realnet_lan_verdict_template.json"
DEFAULT_OUTPUT = "logs/realnet_lan_verdict.json"
REQUIRED_GROUP_FIELDS = {
    "browser_join": (
        "client_can_list_roomd_rooms_ok",
        "public_room_visible_ok",
        "join_attempt_path_ok",
        "selected_transport_shown_ok",
        "host_and_client_reach_same_lobby_ok",
        "host_start_enters_gameplay_ok",
        "client_input_moves_player_ok",
        "no_desync_or_frozen_join_ok",
    ),
    "direct_join": (
        "direct_ip_join_ok",
        "direct_join_enters_gameplay_ok",
    ),
}
REQUIRED_TOP_LEVEL_STRINGS = (
    "host_commit",
    "client_commit",
    "room_server_url",
)


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def ask_bool(prompt: str, current: bool) -> bool:
    suffix = "[Y/n]" if current else "[y/N]"
    while True:
        answer = input(f"{prompt} {suffix} ").strip().lower()
        if not answer:
            return current
        if answer in ("y", "yes", "true", "1"):
            return True
        if answer in ("n", "no", "false", "0"):
            return False
        print("Please answer y or n.")


def ask_text(prompt: str, current: str) -> str:
    if current:
        print(f"{prompt} current: {current}")
    answer = input(f"{prompt} (blank keeps current) ").rstrip()
    return current if answer == "" else answer


def recompute_ok(verdict: dict[str, Any]) -> bool:
    for field in REQUIRED_TOP_LEVEL_STRINGS:
        if not str(verdict.get(field, "")).strip():
            return False
    for group_name, fields in REQUIRED_GROUP_FIELDS.items():
        group = verdict.get(group_name)
        if not isinstance(group, dict):
            return False
        if any(group.get(field, False) is not True for field in fields):
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prompt for the Realnet LAN validation verdict and write JSON."
    )
    parser.add_argument("--template", default=DEFAULT_TEMPLATE)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    template_path = Path(args.template)
    output_path = Path(args.output)
    source_path = (
        output_path
        if output_path.exists() and output_path.stat().st_size > 0
        else template_path
    )
    verdict = load_json(source_path)

    print("Fill this only after running the two-machine Realnet LAN validation.")
    print("Blank answers keep the current value.")
    print("")

    for field in REQUIRED_TOP_LEVEL_STRINGS:
        verdict[field] = ask_text(field, str(verdict.get(field, "")))
    print("")

    for group_name, fields in REQUIRED_GROUP_FIELDS.items():
        group = verdict.setdefault(group_name, {})
        if not isinstance(group, dict):
            raise SystemExit(f"group `{group_name}` must be an object")
        print(group_name)
        for field in fields:
            group[field] = ask_bool(field, bool(group.get(field, False)))
        group["notes"] = ask_text("notes", str(group.get("notes", "")))
        print("")

    verdict["overall_notes"] = ask_text("overall_notes", str(verdict.get("overall_notes", "")))
    verdict["ok"] = recompute_ok(verdict)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output_path}")
    print(f"Verify with: scripts/summarize_realnet_lan_validation.py --verdict-json {output_path}")
    print("Full gate: scripts/audit_realnet_foundation.sh")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
