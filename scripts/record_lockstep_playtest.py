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
DEFAULT_OUTPUT = "logs/lockstep_playtest_samples.jsonl"
DEFAULT_SUMMARY = "logs/lockstep_playtest_summary.json"


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
            "join_barrier_seen": False,
            "max_sim_ms": 0.0,
            "max_hash_ms": 0.0,
            "max_rollback_replay_ms": 0.0,
            "last_stage": "",
            "last_frame": 0,
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
        endpoint_summary["join_barrier_seen"] = (
            endpoint_summary["join_barrier_seen"] or
            bool(net.get("join_barrier", {}).get("active", False))
        )
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
        "--endpoint",
        action="append",
        type=parse_endpoint,
        help="endpoint to sample as name:port or name:host:port; repeatable",
    )
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument("--summary-json", default=DEFAULT_SUMMARY)
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

    endpoints = args.endpoint if args.endpoint else default_endpoints()
    output_path = Path(args.output)
    summary_path = Path(args.summary_json)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    started_at = time.time()
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
