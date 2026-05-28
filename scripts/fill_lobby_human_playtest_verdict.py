#!/usr/bin/env python3
"""Interactively fill the lobby human-playtest verdict JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_TEMPLATE = "docs/lobby_human_playtest_verdict_template.json"
DEFAULT_OUTPUT = "logs/lobby_human_playtest_verdict.json"
REQUIRED_GROUP_FIELDS = {
    "public_browser_flow": (
        "host_fields_labeled_ok",
        "host_public_bottom_action_ok",
        "browser_search_and_refresh_ok",
        "joined_actions_gated_ok",
        "wait_then_play_ok",
        "client_enters_gameplay_or_loading_ok",
        "client_input_moves_player_ok",
    ),
    "direct_join_flow": (
        "join_fields_labeled_ok",
        "no_server_stays_on_join_by_ip_ok",
        "no_false_joined_state_ok",
        "direct_success_ok",
        "joined_state_truthful_ok",
    ),
    "alerts": (
        "join_failure_success_alerts_ok",
        "player_join_leave_alerts_ok",
        "host_start_alert_ok",
        "gameplay_menu_closed_alerts_ok",
        "stacking_readability_ok",
    ),
}


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
    for group_name, fields in REQUIRED_GROUP_FIELDS.items():
        group = verdict.get(group_name)
        if not isinstance(group, dict):
            return False
        if any(group.get(field, False) is not True for field in fields):
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prompt for the human lobby playtest verdict and write JSON."
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

    print("Fill this only after actually running the lobby human playtest checklist.")
    print("Blank answers keep the current value.")
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
    print(f"Verify with: scripts/summarize_lobby_human_playtest.py --verdict-json {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
