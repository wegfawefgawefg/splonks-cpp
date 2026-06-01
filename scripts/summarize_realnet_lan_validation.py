#!/usr/bin/env python3
"""Audit the manual Realnet LAN validation verdict artifact."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_VERDICT_JSON = "logs/realnet_lan_verdict.json"
DEFAULT_REPORT_JSON = "logs/realnet_lan_audit.json"
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


def evaluate(verdict: dict[str, Any]) -> dict[str, Any]:
    problems: list[str] = []
    groups: dict[str, Any] = {}

    if verdict.get("ok", False) is not True:
        problems.append("ok=false")

    for field in REQUIRED_TOP_LEVEL_STRINGS:
        if not str(verdict.get(field, "")).strip():
            problems.append(f"{field}=empty")

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
        "host_commit": verdict.get("host_commit", ""),
        "client_commit": verdict.get("client_commit", ""),
        "room_server_url": verdict.get("room_server_url", ""),
        "overall_notes": verdict.get("overall_notes", ""),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit the filled Realnet LAN validation verdict JSON."
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

    print(f"realnet LAN validation: {'ok' if report['ok'] else 'FAIL'}")
    print(f"report: {report_path}")
    for problem in report["problems"]:
        print(f"  - {problem}")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
