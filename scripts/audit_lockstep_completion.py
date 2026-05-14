#!/usr/bin/env python3
"""Audit whether the lockstep rollback plan has final completion evidence."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


DEFAULT_VALIDATION_REPORT = "logs/lockstep_validate_report.json"
DEFAULT_VERDICT_JSON = "logs/lockstep_playtest_verdict.json"
DEFAULT_VERDICT_TEMPLATE = "docs/plans/lockstep_human_playtest_verdict_template.json"
DEFAULT_SUMMARY_GLOB = "logs/lockstep_playtest_*_summary.json"
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


def run_human_audit(
    verdict_json: Path,
    verdict_template: Path,
    summary_glob: str,
    min_duration: float,
) -> tuple[bool, str]:
    missing_verdict = not verdict_json.exists()
    audit_verdict = verdict_json if not missing_verdict else verdict_template
    if not audit_verdict.exists():
        return False, f"missing verdict JSON: {verdict_json}\nmissing verdict template: {verdict_template}"
    cmd = [
        "scripts/summarize_lockstep_playtest.py",
        "--min-duration",
        str(min_duration),
        "--glob",
        summary_glob,
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


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def synthetic_endpoint() -> dict[str, Any]:
    return {
        "error_count": 0,
        "fatal_desync_seen": False,
        "join_barrier_active_samples": 0,
        "max_hash_mismatches": 0,
        "max_hash_ms": 0.1,
        "max_rollback_replay_ms": 0.1,
        "max_sim_ms": 0.5,
        "recovery_mode_counts": {},
        "snapshot_resync_active_samples": 0,
    }


def write_self_test_artifacts(root: Path) -> tuple[Path, Path, str]:
    validation_report = root / "lockstep_validate_report.json"
    verdict_json = root / "lockstep_playtest_verdict.json"
    try:
        summary_root = root.relative_to(Path.cwd())
    except ValueError:
        summary_root = root
    summary_glob = str(summary_root / "lockstep_playtest_*_summary.json")

    write_json(
        validation_report,
        {
            "ok": True,
            "profiles": list(DEFAULT_REQUIRED_VALIDATION_PROFILES),
            "repeat": 1,
            "results": [
                {
                    "profile": profile,
                    "run_index": 1,
                    "repeat_count": 1,
                    "ok": True,
                    "problems": [],
                    "host": {"hash_mismatches": 0},
                    "peer": {"hash_mismatches": 0},
                }
                for profile in DEFAULT_REQUIRED_VALIDATION_PROFILES
            ],
        },
    )

    for profile in ("same-house", "tx-ca", "tx-japan"):
        write_json(
            root / f"lockstep_playtest_20000101_000000_{profile}_summary.json",
            {
                "ok": True,
                "profile": profile,
                "started_at_unix": 1.0,
                "duration_s": 300.0,
                "sample_count": 300,
                "endpoints": {
                    "host": synthetic_endpoint(),
                    "peer": synthetic_endpoint(),
                },
            },
        )

    verdict_profiles: dict[str, dict[str, Any]] = {}
    for profile in ("same-house", "tx-ca", "tx-japan"):
        profile_verdict = {
            "movement_ok": True,
            "hang_jump_climb_ok": True,
            "carry_throw_ok": True,
            "tools_weapons_ok": True,
            "explosives_tiles_ok": True,
            "stage_transition_ok": True,
            "feel_ok": True,
            "notes": "synthetic self-test verdict",
        }
        if profile == "tx-japan":
            profile_verdict["high_latency_feel_ok"] = True
        verdict_profiles[profile] = profile_verdict
    write_json(
        verdict_json,
        {
            "ok": True,
            "profiles": verdict_profiles,
            "default_delay_prediction_ok": True,
            "overall_notes": "synthetic self-test verdict",
        },
    )
    return validation_report, verdict_json, summary_glob


def run_self_test() -> int:
    with tempfile.TemporaryDirectory(prefix=".splonks_lockstep_audit_", dir=".") as tmp:
        validation_report, verdict_json, summary_glob = write_self_test_artifacts(Path(tmp))
        cmd = [
            sys.executable,
            __file__,
            "--validation-report",
            str(validation_report),
            "--verdict-json",
            str(verdict_json),
            "--summary-glob",
            summary_glob,
        ]
        completed = subprocess.run(
            cmd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        print(completed.stdout, end="")
        if completed.returncode != 0:
            print("audit self-test: FAIL")
            return completed.returncode
    print("audit self-test: PASS")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit all objective evidence for the lockstep completion gate."
    )
    parser.add_argument("--validation-report", default=DEFAULT_VALIDATION_REPORT)
    parser.add_argument("--verdict-json", default=DEFAULT_VERDICT_JSON)
    parser.add_argument("--verdict-template", default=DEFAULT_VERDICT_TEMPLATE)
    parser.add_argument("--summary-glob", default=DEFAULT_SUMMARY_GLOB)
    parser.add_argument("--min-duration", type=float, default=240.0)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()

    validation_problems = audit_validation_report(Path(args.validation_report))
    human_ok, human_output = run_human_audit(
        Path(args.verdict_json),
        Path(args.verdict_template),
        args.summary_glob,
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
