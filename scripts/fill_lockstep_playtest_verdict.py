#!/usr/bin/env python3
"""Interactively fill the lockstep human-playtest verdict JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_TEMPLATE = "docs/plans/lockstep_human_playtest_verdict_template.json"
DEFAULT_OUTPUT = "logs/lockstep_playtest_verdict.json"
REQUIRED_FIELDS = (
    "movement_ok",
    "hang_jump_climb_ok",
    "carry_throw_ok",
    "tools_weapons_ok",
    "explosives_tiles_ok",
    "stage_transition_ok",
    "feel_ok",
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


def profile_ok(profile: dict[str, Any], name: str) -> bool:
    if any(profile.get(field, False) is not True for field in REQUIRED_FIELDS):
        return False
    if name == "tx-japan" and profile.get("high_latency_feel_ok", False) is not True:
        return False
    return True


def recompute_ok(verdict: dict[str, Any]) -> bool:
    if verdict.get("default_delay_prediction_ok", False) is not True:
        return False
    profile_map = verdict.get("profiles", {})
    if not isinstance(profile_map, dict):
        return False
    if not profile_map:
        return False
    for name, profile in profile_map.items():
        if not isinstance(profile, dict) or not profile_ok(profile, name):
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prompt for the human lockstep playtest verdict and write JSON."
    )
    parser.add_argument("--template", default=DEFAULT_TEMPLATE)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--profile",
        action="append",
        dest="profiles",
        help="profile to fill; repeatable. Defaults to profiles in the template.",
    )
    args = parser.parse_args()

    template_path = Path(args.template)
    output_path = Path(args.output)
    source_path = (
        output_path
        if output_path.exists() and output_path.stat().st_size > 0
        else template_path
    )
    verdict = load_json(source_path)
    profile_map = verdict.setdefault("profiles", {})
    if not isinstance(profile_map, dict):
        raise SystemExit("verdict profiles must be an object")

    profiles = tuple(args.profiles) if args.profiles else tuple(profile_map.keys())
    if not profiles:
        raise SystemExit("no profiles to fill")

    print("Fill this only after actually running the human playtest checklist.")
    print("Blank answers keep the current value.")
    print("")

    for name in profiles:
        profile = profile_map.setdefault(name, {})
        if not isinstance(profile, dict):
            raise SystemExit(f"profile `{name}` must be an object")
        print(f"Profile: {name}")
        for field in REQUIRED_FIELDS:
            profile[field] = ask_bool(field, bool(profile.get(field, False)))
        if name == "tx-japan":
            profile["high_latency_feel_ok"] = ask_bool(
                "high_latency_feel_ok",
                bool(profile.get("high_latency_feel_ok", False)),
            )
        profile["notes"] = ask_text("notes", str(profile.get("notes", "")))
        print("")

    verdict["default_delay_prediction_ok"] = ask_bool(
        "default_delay_prediction_ok",
        bool(verdict.get("default_delay_prediction_ok", False)),
    )
    verdict["overall_notes"] = ask_text("overall_notes", str(verdict.get("overall_notes", "")))
    verdict["ok"] = recompute_ok(verdict)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output_path}")
    print(
        "Verify with: "
        f"scripts/summarize_lockstep_playtest.py --verdict-json {output_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
