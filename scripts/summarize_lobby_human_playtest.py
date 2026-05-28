#!/usr/bin/env python3
"""Audit the manual lobby playtest verdict artifact."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_VERDICT_JSON = "logs/lobby_human_playtest_verdict.json"
DEFAULT_REPORT_JSON = "logs/lobby_human_playtest_audit.json"
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


def evaluate(verdict: dict[str, Any]) -> dict[str, Any]:
    problems: list[str] = []
    groups: dict[str, Any] = {}

    if verdict.get("ok", False) is not True:
        problems.append("ok=false")

    for group_name, fields in REQUIRED_GROUP_FIELDS.items():
        group = verdict.get(group_name)
        group_result: dict[str, Any] = {"ok": False, "problems": []}
        if not isinstance(group, dict):
            group_result["problems"].append("missing group")
        else:
            for field in fields:
                if group.get(field, False) is not True:
                    group_result["problems"].append(f"{field}=false")
            group_result["notes"] = group.get("notes", "")
        group_result["ok"] = not group_result["problems"]
        groups[group_name] = group_result
        problems.extend(
            f"{group_name}: {problem}" for problem in group_result["problems"]
        )

    return {
        "ok": not problems,
        "problems": problems,
        "groups": groups,
        "overall_notes": verdict.get("overall_notes", ""),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit the filled lobby human-playtest verdict JSON."
    )
    parser.add_argument("--verdict-json", default=DEFAULT_VERDICT_JSON)
    parser.add_argument("--report-json", default=DEFAULT_REPORT_JSON)
    args = parser.parse_args()

    verdict_path = Path(args.verdict_json)
    if not verdict_path.exists():
        raise SystemExit(f"missing verdict JSON: {verdict_path}")

    report = evaluate(load_json(verdict_path))
    report_path = Path(args.report_json)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"lobby human playtest: {'ok' if report['ok'] else 'FAIL'}")
    print(f"report: {report_path}")
    for problem in report["problems"]:
        print(f"  - {problem}")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
