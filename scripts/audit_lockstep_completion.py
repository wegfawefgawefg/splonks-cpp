#!/usr/bin/env python3
"""Audit whether the lockstep rollback plan has final completion evidence."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


DEFAULT_VALIDATION_REPORT = "logs/lockstep_validate_report.json"
DEFAULT_VERDICT_JSON = "logs/lockstep_playtest_verdict.json"
DEFAULT_VERDICT_TEMPLATE = "docs/plans/lockstep_human_playtest_verdict_template.json"
DEFAULT_REQUIRED_VALIDATION_PROFILES = (
    "same-house",
    "same-city",
    "same-state",
    "tx-ca",
    "ca-fl",
    "us-cross-country",
    "tx-japan",
)


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def audit_validation_report(path: Path) -> list[str]:
    problems: list[str] = []
    if not path.exists():
        return [f"missing validation report: {path}"]
    try:
        report = load_json(path)
    except Exception as exc:
        return [f"could not read validation report {path}: {exc}"]

    if report.get("ok") is not True:
        problems.append("validation report ok=false")
    profiles = report.get("profiles", [])
    for profile in DEFAULT_REQUIRED_VALIDATION_PROFILES:
        if profile not in profiles:
            problems.append(f"validation report missing profile {profile}")

    results = report.get("results", [])
    if not isinstance(results, list):
        problems.append("validation report results is not a list")
        return problems
    by_profile = {str(result.get("profile", "")): result for result in results}
    for profile in DEFAULT_REQUIRED_VALIDATION_PROFILES:
        result = by_profile.get(profile)
        if not isinstance(result, dict):
            problems.append(f"validation report missing result for {profile}")
            continue
        if result.get("ok") is not True:
            problems.append(f"validation profile {profile} ok=false: {result.get('problems', [])}")
        for side in ("host", "peer"):
            endpoint = result.get(side, {})
            if not isinstance(endpoint, dict):
                problems.append(f"validation profile {profile} missing {side} endpoint")
                continue
            if int(endpoint.get("hash_mismatches", 0)) != 0:
                problems.append(f"validation profile {profile} {side} hash mismatch")
    return problems


def run_human_audit(verdict_json: Path, verdict_template: Path, min_duration: float) -> tuple[bool, str]:
    missing_verdict = not verdict_json.exists()
    audit_verdict = verdict_json if not missing_verdict else verdict_template
    if not audit_verdict.exists():
        return False, f"missing verdict JSON: {verdict_json}\nmissing verdict template: {verdict_template}"
    cmd = [
        "scripts/summarize_lockstep_playtest.py",
        "--min-duration",
        str(min_duration),
        "--verdict-json",
        str(audit_verdict),
    ]
    completed = subprocess.run(
        cmd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    output = completed.stdout
    if missing_verdict:
        output = f"missing verdict JSON: {verdict_json}\n{output}"
    return completed.returncode == 0 and not missing_verdict, output


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit all objective evidence for the lockstep completion gate."
    )
    parser.add_argument("--validation-report", default=DEFAULT_VALIDATION_REPORT)
    parser.add_argument("--verdict-json", default=DEFAULT_VERDICT_JSON)
    parser.add_argument("--verdict-template", default=DEFAULT_VERDICT_TEMPLATE)
    parser.add_argument("--min-duration", type=float, default=240.0)
    args = parser.parse_args()

    validation_problems = audit_validation_report(Path(args.validation_report))
    human_ok, human_output = run_human_audit(
        Path(args.verdict_json),
        Path(args.verdict_template),
        args.min_duration,
    )

    if not validation_problems and human_ok:
        print("lockstep completion audit: PASS")
        print(f"  validation report: {args.validation_report}")
        print(f"  verdict JSON: {args.verdict_json}")
        return 0

    print("lockstep completion audit: FAIL")
    if validation_problems:
        print("  validation report:")
        for problem in validation_problems:
            print(f"    problem: {problem}")
    else:
        print(f"  validation report: PASS ({args.validation_report})")

    print("  human playtest/verdict:")
    for line in human_output.splitlines() if human_output else [f"missing verdict JSON: {args.verdict_json}"]:
        print(f"    {line}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
