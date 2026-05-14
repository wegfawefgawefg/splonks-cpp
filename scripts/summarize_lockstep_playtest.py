#!/usr/bin/env python3
"""Summarize lockstep human-playtest telemetry artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


DEFAULT_REQUIRED_PROFILES = ("same-house", "tx-ca", "tx-japan")
DEFAULT_SUMMARY_GLOB = "logs/lockstep_playtest_*_summary.json"
DEFAULT_REPORT = "logs/lockstep_playtest_audit.json"
DEFAULT_MIN_DURATION_S = 240.0
DEFAULT_MAX_SIM_MS = 8.0
DEFAULT_MAX_HASH_MS = 4.0
DEFAULT_MAX_ROLLBACK_REPLAY_MS = 4.0
REQUIRED_VERDICT_FIELDS = (
    "movement_ok",
    "hang_jump_climb_ok",
    "carry_throw_ok",
    "tools_weapons_ok",
    "explosives_tiles_ok",
    "stage_transition_ok",
    "feel_ok",
)


def load_summary(path: Path) -> dict[str, Any] | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        data["_path"] = str(path)
        return data
    except Exception as exc:
        return {
            "_path": str(path),
            "ok": False,
            "profile": "",
            "started_at_unix": 0.0,
            "problems": [f"could not read summary: {exc}"],
            "endpoints": {},
        }


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def latest_by_profile(summaries: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    latest: dict[str, dict[str, Any]] = {}
    for summary in summaries:
        profile = str(summary.get("profile", ""))
        if not profile:
            continue
        current = latest.get(profile)
        if current is None or float(summary.get("started_at_unix", 0.0)) > float(
            current.get("started_at_unix", 0.0)
        ):
            latest[profile] = summary
    return latest


def endpoint_problems(
    name: str,
    endpoint: dict[str, Any],
    max_sim_ms: float,
    max_hash_ms: float,
    max_rollback_replay_ms: float,
    allow_snapshot_resync: bool,
    allow_join_barrier: bool,
) -> list[str]:
    problems: list[str] = []
    if int(endpoint.get("error_count", 0)) != 0:
        problems.append(f"{name}: control errors={endpoint.get('error_count')}")
    if int(endpoint.get("max_hash_mismatches", 0)) != 0:
        problems.append(f"{name}: hash mismatches={endpoint.get('max_hash_mismatches')}")
    if bool(endpoint.get("fatal_desync_seen", False)):
        problems.append(f"{name}: fatal desync seen")
    if not allow_snapshot_resync and int(endpoint.get("snapshot_resync_active_samples", 0)) != 0:
        problems.append(
            f"{name}: snapshot resync samples={endpoint.get('snapshot_resync_active_samples')}"
        )
    if not allow_join_barrier and int(endpoint.get("join_barrier_active_samples", 0)) != 0:
        problems.append(
            f"{name}: join barrier samples={endpoint.get('join_barrier_active_samples')}"
        )
    if float(endpoint.get("max_sim_ms", 0.0)) > max_sim_ms:
        problems.append(f"{name}: sim ms={endpoint.get('max_sim_ms')} > {max_sim_ms}")
    if float(endpoint.get("max_hash_ms", 0.0)) > max_hash_ms:
        problems.append(f"{name}: hash ms={endpoint.get('max_hash_ms')} > {max_hash_ms}")
    if float(endpoint.get("max_rollback_replay_ms", 0.0)) > max_rollback_replay_ms:
        problems.append(
            f"{name}: rollback replay ms={endpoint.get('max_rollback_replay_ms')} > "
            f"{max_rollback_replay_ms}"
        )
    return problems


def evaluate_profile(
    profile: str,
    summary: dict[str, Any] | None,
    min_duration_s: float,
    max_sim_ms: float,
    max_hash_ms: float,
    max_rollback_replay_ms: float,
    allow_snapshot_resync: bool,
    allow_join_barrier: bool,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "profile": profile,
        "ok": False,
        "problems": [],
    }
    if summary is None:
        result["problems"].append("missing summary")
        return result

    result["path"] = summary.get("_path", "")
    result["started_at_unix"] = summary.get("started_at_unix", 0.0)
    result["duration_s"] = summary.get("duration_s", 0.0)
    result["sample_count"] = summary.get("sample_count", 0)
    result["summary_ok"] = summary.get("ok", False)
    if summary.get("ok", False) is not True:
        result["problems"].append("summary ok=false")
    if float(summary.get("duration_s", 0.0)) < min_duration_s:
        result["problems"].append(
            f"duration {summary.get('duration_s', 0.0):.1f}s < {min_duration_s:.1f}s"
        )
    endpoints = summary.get("endpoints", {})
    if not endpoints:
        result["problems"].append("no endpoint summaries")
    endpoint_results: dict[str, Any] = {}
    for name, endpoint in endpoints.items():
        problems = endpoint_problems(
            str(name),
            endpoint,
            max_sim_ms,
            max_hash_ms,
            max_rollback_replay_ms,
            allow_snapshot_resync,
            allow_join_barrier,
        )
        endpoint_results[str(name)] = {
            "ok": not problems,
            "problems": problems,
            "last_stage": endpoint.get("last_stage", ""),
            "last_frame": endpoint.get("last_frame", 0),
            "max_sim_ms": endpoint.get("max_sim_ms", 0.0),
            "max_hash_ms": endpoint.get("max_hash_ms", 0.0),
            "max_rollback_replay_ms": endpoint.get("max_rollback_replay_ms", 0.0),
            "max_rollbacks_per_second": endpoint.get("max_rollbacks_per_second", 0.0),
            "max_prediction_miss_count": endpoint.get("max_prediction_miss_count", 0),
            "max_arbitrated_missing_input_count": endpoint.get(
                "max_arbitrated_missing_input_count",
                0,
            ),
            "recovery_mode_counts": endpoint.get("recovery_mode_counts", {}),
        }
        result["problems"].extend(problems)
    result["endpoints"] = endpoint_results
    result["ok"] = not result["problems"]
    return result


def evaluate_verdict(
    verdict: dict[str, Any] | None,
    required_profiles: tuple[str, ...],
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "ok": False,
        "problems": [],
        "profiles": {},
    }
    if verdict is None:
        result["problems"].append("missing manual verdict")
        return result
    result["raw_ok"] = verdict.get("ok", False)
    if verdict.get("ok", False) is not True:
        result["problems"].append("manual verdict ok=false")
    if verdict.get("default_delay_prediction_ok", False) is not True:
        result["problems"].append("default_delay_prediction_ok=false")

    profiles = verdict.get("profiles", {})
    if not isinstance(profiles, dict):
        result["problems"].append("manual verdict profiles is not an object")
        return result

    profile_results: dict[str, Any] = {}
    for profile in required_profiles:
        profile_verdict = profiles.get(profile)
        profile_result: dict[str, Any] = {
            "ok": False,
            "problems": [],
        }
        if not isinstance(profile_verdict, dict):
            profile_result["problems"].append("missing profile verdict")
        else:
            for field in REQUIRED_VERDICT_FIELDS:
                if profile_verdict.get(field, False) is not True:
                    profile_result["problems"].append(f"{field}=false")
            if (
                profile == "tx-japan" and
                profile_verdict.get("high_latency_feel_ok", False) is not True
            ):
                profile_result["problems"].append("high_latency_feel_ok=false")
            profile_result["notes"] = profile_verdict.get("notes", "")
        profile_result["ok"] = not profile_result["problems"]
        profile_results[profile] = profile_result
        result["problems"].extend(f"{profile}: {problem}" for problem in profile_result["problems"])

    result["profiles"] = profile_results
    result["ok"] = not result["problems"]
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit lockstep playtest recorder summaries."
    )
    parser.add_argument("--glob", default=DEFAULT_SUMMARY_GLOB)
    parser.add_argument(
        "--required-profile",
        action="append",
        dest="required_profiles",
        help="required profile; repeatable. Defaults to same-house, tx-ca, tx-japan.",
    )
    parser.add_argument("--min-duration", type=float, default=DEFAULT_MIN_DURATION_S)
    parser.add_argument("--max-sim-ms", type=float, default=DEFAULT_MAX_SIM_MS)
    parser.add_argument("--max-hash-ms", type=float, default=DEFAULT_MAX_HASH_MS)
    parser.add_argument(
        "--max-rollback-replay-ms",
        type=float,
        default=DEFAULT_MAX_ROLLBACK_REPLAY_MS,
    )
    parser.add_argument("--allow-snapshot-resync", action="store_true")
    parser.add_argument("--allow-join-barrier", action="store_true")
    parser.add_argument(
        "--verdict-json",
        default=None,
        help="optional filled human verdict JSON; required for final completion audit",
    )
    parser.add_argument("--report-json", default=DEFAULT_REPORT)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if args.min_duration < 0.0:
        parser.error("--min-duration must be non-negative")
    required_profiles = tuple(args.required_profiles) if args.required_profiles else DEFAULT_REQUIRED_PROFILES
    summaries = [
        summary
        for path in sorted(Path().glob(args.glob))
        if (summary := load_summary(path)) is not None
    ]
    latest = latest_by_profile(summaries)
    profile_results = [
        evaluate_profile(
            profile,
            latest.get(profile),
            args.min_duration,
            args.max_sim_ms,
            args.max_hash_ms,
            args.max_rollback_replay_ms,
            args.allow_snapshot_resync,
            args.allow_join_barrier,
        )
        for profile in required_profiles
    ]
    verdict_result = None
    if args.verdict_json:
        try:
            verdict_result = evaluate_verdict(
                load_json(Path(args.verdict_json)),
                required_profiles,
            )
        except Exception as exc:
            verdict_result = {
                "ok": False,
                "problems": [f"could not read manual verdict: {exc}"],
                "profiles": {},
            }
    report = {
        "ok": (
            all(result["ok"] for result in profile_results) and
            (verdict_result is None or verdict_result["ok"])
        ),
        "glob": args.glob,
        "required_profiles": list(required_profiles),
        "summary_count": len(summaries),
        "profile_results": profile_results,
        "verdict": verdict_result,
    }
    report_path = Path(args.report_json)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"playtest audit: {'ok' if report['ok'] else 'FAIL'}")
        for result in profile_results:
            print(
                f"  {result['profile']}: {'ok' if result['ok'] else 'FAIL'} "
                f"path={result.get('path', '<missing>')}"
            )
            for problem in result["problems"]:
                print(f"    problem: {problem}")
        if verdict_result is not None:
            print(f"  manual verdict: {'ok' if verdict_result['ok'] else 'FAIL'}")
            for problem in verdict_result["problems"]:
                print(f"    problem: {problem}")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
