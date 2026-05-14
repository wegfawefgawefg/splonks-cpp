#!/usr/bin/env python3
"""Record live lockstep telemetry during a human playtest session."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_HOST_PORT = 41000
DEFAULT_PEER_PORT = 41001


@dataclass(frozen=True)
class Endpoint:
    name: str
    host: str
    port: int


def control(endpoint: Endpoint, command: str, timeout: float = 2.0) -> dict[str, Any]:
    with socket.create_connection((endpoint.host, endpoint.port), timeout=timeout) as sock:
        sock.sendall((command + "\n").encode("utf-8"))
        chunks: list[bytes] = []
        while True:
            data = sock.recv(65536)
            if not data:
                break
            chunks.append(data)
    raw = b"".join(chunks).decode("utf-8", errors="replace").strip()
    parsed = json.loads(raw)
    if not parsed.get("ok", False):
        raise RuntimeError(f"{endpoint.name}: `{command}` failed: {parsed}")
    return parsed


def collect_endpoint(
    endpoint: Endpoint,
    include_fingerprint: bool,
) -> tuple[dict[str, Any], str | None]:
    try:
        data = {
            "net": control(endpoint, "net"),
            "perf": control(endpoint, "perf"),
            "players": control(endpoint, "players"),
        }
        if include_fingerprint:
            data["fingerprint"] = control(endpoint, "fingerprint")
        return data, None
    except Exception as exc:
        return {}, str(exc)


def is_ready(net: dict[str, Any]) -> bool:
    barrier = net.get("join_barrier", {})
    snapshot_resync = net.get("snapshot_resync", {})
    return (
        net.get("input_lockstep_enabled") is True and
        barrier.get("active") is False and
        barrier.get("phase") in (None, "none") and
        not bool(snapshot_resync.get("pending_request", False)) and
        not bool(snapshot_resync.get("waiting_for_ack", False)) and
        int(snapshot_resync.get("queued_targets", 0)) == 0 and
        int(snapshot_resync.get("active_transfer_id", 0)) == 0 and
        net.get("lockstep_last_desync_recovery_mode") != "fatal-desync"
    )


def wait_ready(endpoints: list[Endpoint], timeout_s: float) -> None:
    deadline = time.monotonic() + timeout_s
    last_error = ""
    while time.monotonic() < deadline:
        try:
            nets = [control(endpoint, "net") for endpoint in endpoints]
            if all(is_ready(net) for net in nets):
                return
            last_error = "; ".join(
                f"{endpoint.name}: barrier={net.get('join_barrier', {}).get('phase')} "
                f"recovery={net.get('lockstep_last_desync_recovery_mode')} "
                f"snapshot={net.get('snapshot_resync', {})}"
                for endpoint, net in zip(endpoints, nets, strict=True)
            )
        except Exception as exc:
            last_error = str(exc)
        time.sleep(0.2)
    raise RuntimeError(f"lockstep session was not ready within {timeout_s:.1f}s: {last_error}")


def apply_profile(endpoints: list[Endpoint], profile: str) -> None:
    for endpoint in endpoints:
        if profile == "off":
            control(endpoint, "net fuzzer off")
        else:
            control(endpoint, f"net fuzzer preset {profile}")


def parse_endpoint(spec: str) -> Endpoint:
    # Accepted forms:
    #   name:port
    #   name:host:port
    parts = spec.split(":")
    if len(parts) == 2:
        name, port = parts
        return Endpoint(name=name, host="127.0.0.1", port=int(port))
    if len(parts) == 3:
        name, host, port = parts
        return Endpoint(name=name, host=host, port=int(port))
    raise argparse.ArgumentTypeError(
        f"invalid endpoint `{spec}`; expected name:port or name:host:port"
    )


def default_endpoints() -> list[Endpoint]:
    return [
        Endpoint("host", "127.0.0.1", DEFAULT_HOST_PORT),
        Endpoint("peer", "127.0.0.1", DEFAULT_PEER_PORT),
    ]


def safe_path_component(text: str) -> str:
    out = []
    for char in text.lower():
        if char.isalnum():
            out.append(char)
        elif char in ("-", "_"):
            out.append(char)
        elif char.isspace():
            out.append("_")
    return "".join(out).strip("_") or "session"


def default_output_paths(profile: str, label: str, started_at: float) -> tuple[Path, Path]:
    timestamp = time.strftime("%Y%m%d_%H%M%S", time.localtime(started_at))
    name_parts = [timestamp]
    if profile:
        name_parts.append(safe_path_component(profile))
    if label:
        name_parts.append(safe_path_component(label))
    stem = "lockstep_playtest_" + "_".join(name_parts)
    return (
        Path("logs") / f"{stem}_samples.jsonl",
        Path("logs") / f"{stem}_summary.json",
    )


def update_summary(summary: dict[str, Any], sample: dict[str, Any]) -> None:
    summary["sample_count"] += 1
    summary["duration_s"] = sample["elapsed_s"]
    for name, endpoint_sample in sample["endpoints"].items():
        endpoint_summary = summary["endpoints"].setdefault(name, {
            "sample_count": 0,
            "error_count": 0,
            "max_hash_mismatches": 0,
            "max_confirmed_hash_lag": 0,
            "fatal_desync_seen": False,
            "recovery_mode_counts": {},
            "snapshot_resync_seen": False,
            "snapshot_resync_active_samples": 0,
            "join_barrier_seen": False,
            "join_barrier_active_samples": 0,
            "max_sim_ms": 0.0,
            "max_hash_ms": 0.0,
            "max_rollback_replay_ms": 0.0,
            "max_rollbacks_per_second": 0.0,
            "max_prediction_miss_count": 0,
            "max_arbitrated_missing_input_count": 0,
            "last_stage": "",
            "last_frame": 0,
            "last_recovery_mode": "none",
        })
        endpoint_summary["sample_count"] += 1
        if endpoint_sample.get("error"):
            endpoint_summary["error_count"] += 1
            summary["ok"] = False
            continue

        data = endpoint_sample["data"]
        net = data["net"]
        perf = data["perf"]
        frame = int(net.get("lockstep_next_frame", 0))
        confirmed_frame = int(net.get("lockstep_last_confirmed_hash_frame", 0))
        confirmed_lag = max(0, frame - confirmed_frame)
        hash_mismatches = int(net.get("lockstep_hash_mismatch_count", 0))
        endpoint_summary["max_hash_mismatches"] = max(
            endpoint_summary["max_hash_mismatches"],
            hash_mismatches,
        )
        endpoint_summary["max_confirmed_hash_lag"] = max(
            endpoint_summary["max_confirmed_hash_lag"],
            confirmed_lag,
        )
        endpoint_summary["fatal_desync_seen"] = (
            endpoint_summary["fatal_desync_seen"] or
            net.get("lockstep_last_desync_recovery_mode") == "fatal-desync"
        )
        recovery_mode = str(net.get("lockstep_last_desync_recovery_mode", "unknown"))
        recovery_mode_counts = endpoint_summary["recovery_mode_counts"]
        recovery_mode_counts[recovery_mode] = recovery_mode_counts.get(recovery_mode, 0) + 1
        endpoint_summary["last_recovery_mode"] = recovery_mode
        snapshot_resync = net.get("snapshot_resync", {})
        snapshot_active = (
            bool(snapshot_resync.get("pending_request", False)) or
            bool(snapshot_resync.get("waiting_for_ack", False)) or
            int(snapshot_resync.get("queued_targets", 0)) > 0 or
            int(snapshot_resync.get("active_transfer_id", 0)) != 0
        )
        endpoint_summary["snapshot_resync_seen"] = (
            endpoint_summary["snapshot_resync_seen"] or snapshot_active
        )
        if snapshot_active:
            endpoint_summary["snapshot_resync_active_samples"] += 1
        endpoint_summary["join_barrier_seen"] = (
            endpoint_summary["join_barrier_seen"] or
            bool(net.get("join_barrier", {}).get("active", False))
        )
        if bool(net.get("join_barrier", {}).get("active", False)):
            endpoint_summary["join_barrier_active_samples"] += 1
        endpoint_summary["max_sim_ms"] = max(
            endpoint_summary["max_sim_ms"],
            float(perf.get("multiplayer_sim_total_smoothed_ms", 0.0)),
        )
        endpoint_summary["max_hash_ms"] = max(
            endpoint_summary["max_hash_ms"],
            float(perf.get("lockstep_hash_smoothed_ms", 0.0)),
        )
        endpoint_summary["max_rollback_replay_ms"] = max(
            endpoint_summary["max_rollback_replay_ms"],
            float(perf.get("rollback_replay_smoothed_ms", 0.0)),
        )
        endpoint_summary["max_rollbacks_per_second"] = max(
            endpoint_summary["max_rollbacks_per_second"],
            float(net.get("lockstep_rollbacks_per_second", 0.0)),
        )
        endpoint_summary["max_prediction_miss_count"] = max(
            endpoint_summary["max_prediction_miss_count"],
            int(net.get("lockstep_prediction_miss_count", 0)),
        )
        endpoint_summary["max_arbitrated_missing_input_count"] = max(
            endpoint_summary["max_arbitrated_missing_input_count"],
            int(net.get("lockstep_arbitrated_missing_input_count", 0)),
        )
        endpoint_summary["last_stage"] = str(net.get("stage", ""))
        endpoint_summary["last_frame"] = frame

        if hash_mismatches != 0 or endpoint_summary["fatal_desync_seen"]:
            summary["ok"] = False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Record live lockstep telemetry while a human playtest runs."
    )
    parser.add_argument("--duration", type=float, default=300.0)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--profile", default="")
    parser.add_argument(
        "--apply-profile",
        action="store_true",
        help="apply --profile as a net fuzzer preset before recording",
    )
    parser.add_argument(
        "--wait-ready",
        action="store_true",
        help="wait for lockstep, join barrier, and snapshot resync to be idle before recording",
    )
    parser.add_argument("--ready-timeout", type=float, default=20.0)
    parser.add_argument(
        "--endpoint",
        action="append",
        type=parse_endpoint,
        help="endpoint to sample as name:port or name:host:port; repeatable",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="sample JSONL path; defaults to logs/lockstep_playtest_<timestamp>_<profile>_samples.jsonl",
    )
    parser.add_argument(
        "--summary-json",
        default=None,
        help="summary JSON path; defaults to logs/lockstep_playtest_<timestamp>_<profile>_summary.json",
    )
    parser.add_argument(
        "--fingerprint-every",
        type=int,
        default=10,
        help="include full fingerprint every N samples; use 0 to disable",
    )
    args = parser.parse_args()
    if args.duration <= 0.0:
        parser.error("--duration must be positive")
    if args.interval <= 0.0:
        parser.error("--interval must be positive")
    if args.fingerprint_every < 0:
        parser.error("--fingerprint-every must be >= 0")
    if args.apply_profile and not args.profile:
        parser.error("--apply-profile requires --profile")

    endpoints = args.endpoint if args.endpoint else default_endpoints()
    if args.wait_ready:
        wait_ready(endpoints, args.ready_timeout)
    if args.apply_profile:
        apply_profile(endpoints, args.profile)
    if args.wait_ready:
        wait_ready(endpoints, args.ready_timeout)

    started_at = time.time()
    default_output_path, default_summary_path = default_output_paths(
        args.profile,
        args.label,
        started_at,
    )
    output_path = Path(args.output) if args.output else default_output_path
    summary_path = Path(args.summary_json) if args.summary_json else default_summary_path
    output_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    monotonic_start = time.monotonic()
    deadline = monotonic_start + args.duration
    summary: dict[str, Any] = {
        "ok": True,
        "label": args.label,
        "profile": args.profile,
        "started_at_unix": started_at,
        "duration_s": 0.0,
        "sample_count": 0,
        "endpoints": {},
        "output": str(output_path),
        "summary": str(summary_path),
    }

    sample_index = 0
    with output_path.open("w", encoding="utf-8") as out:
        while time.monotonic() < deadline:
            now = time.monotonic()
            include_fingerprint = (
                args.fingerprint_every > 0 and
                sample_index % args.fingerprint_every == 0
            )
            sample: dict[str, Any] = {
                "label": args.label,
                "profile": args.profile,
                "sample_index": sample_index,
                "elapsed_s": now - monotonic_start,
                "time_unix": time.time(),
                "endpoints": {},
            }
            for endpoint in endpoints:
                data, error = collect_endpoint(endpoint, include_fingerprint)
                sample["endpoints"][endpoint.name] = {
                    "host": endpoint.host,
                    "port": endpoint.port,
                    "data": data,
                    "error": error,
                }
            update_summary(summary, sample)
            out.write(json.dumps(sample, sort_keys=True) + "\n")
            out.flush()
            sample_index += 1
            sleep_s = args.interval - (time.monotonic() - now)
            if sleep_s > 0.0:
                time.sleep(sleep_s)

    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
