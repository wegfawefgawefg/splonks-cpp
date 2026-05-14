#!/usr/bin/env python3
"""Run a small live lockstep validation pass against running Splonks windows.

This is intentionally a control-server harness, not a replacement for manual
feel testing. It exercises the real UDP/session path and records the same
telemetry the lockstep plan asks for before a human tunes latency feel.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from dataclasses import dataclass
from typing import Any


DEFAULT_HOST_PORT = 41000
DEFAULT_PEER_PORT = 41001
DEFAULT_PROFILES = ("same-house", "tx-ca", "tx-japan")


class ControlError(RuntimeError):
    pass


@dataclass(frozen=True)
class Endpoint:
    name: str
    port: int
    host: str = "127.0.0.1"


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
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ControlError(f"{endpoint.name}: invalid JSON for `{command}`: {raw}") from exc
    if not parsed.get("ok", False):
        raise ControlError(f"{endpoint.name}: `{command}` failed: {parsed}")
    return parsed


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
                f"enabled={net.get('input_lockstep_enabled')}"
                for endpoint, net in zip(endpoints, nets, strict=True)
            )
        except Exception as exc:  # keep waiting while processes boot
            last_error = str(exc)
        time.sleep(0.2)
    raise ControlError(f"lockstep session was not ready within {timeout_s:.1f}s: {last_error}")


def is_ready(net: dict[str, Any]) -> bool:
    barrier = net.get("join_barrier", {})
    return (
        net.get("input_lockstep_enabled") is True
        and barrier.get("active") is False
        and barrier.get("phase") in (None, "none")
        and net.get("lockstep_last_desync_recovery_mode") != "fatal-desync"
    )


def apply_profile(endpoints: list[Endpoint], profile: str) -> None:
    for endpoint in endpoints:
        if profile == "off":
            control(endpoint, "net fuzzer off")
        else:
            control(endpoint, f"net fuzzer preset {profile}")


def inject_many(actions: list[tuple[Endpoint, int, tuple[str, ...]]]) -> None:
    max_frames = 0
    for endpoint, frames, buttons in actions:
        max_frames = max(max_frames, frames)
        suffix = " ".join(buttons) if buttons else "none"
        control(endpoint, f"input {frames} {suffix}")
    time.sleep(max_frames / 60.0 + 0.15)


def run_action_sequence(host: Endpoint, peer: Endpoint) -> None:
    # Movement, jump, hang-ish edge pressure, tool buttons, explosives, and
    # carry/grab input all route through normal local input capture.
    sequence: list[list[tuple[Endpoint, int, tuple[str, ...]]]] = [
        [(host, 45, ("right", "run")), (peer, 45, ("left", "run"))],
        [(host, 18, ("right", "run", "jump")), (peer, 18, ("left", "run", "jump"))],
        [(host, 30, ("left",)), (peer, 30, ("right",))],
        [(host, 12, ("pickup",)), (peer, 12, ("pickup",))],
        [(host, 12, ("attack",)), (peer, 12, ("attack",))],
        [(host, 12, ("rope",)), (peer, 12, ("rope",))],
        [(host, 12, ("bomb",)), (peer, 12, ("bomb",))],
        [(host, 30, ("right", "run")), (peer, 30, ("right", "run"))],
        [(host, 8, ("jump",)), (peer, 8, ("jump",))],
        [(host, 1, tuple()), (peer, 1, tuple())],
    ]
    for step in sequence:
        inject_many(step)
    # Give explosions, rollback repair, and hash exchange time to settle.
    time.sleep(2.0)


def collect(endpoint: Endpoint) -> dict[str, Any]:
    return {
        "net": control(endpoint, "net"),
        "perf": control(endpoint, "perf"),
        "fingerprint": control(endpoint, "fingerprint"),
        "players": control(endpoint, "players"),
    }


def summarize_profile(profile: str, samples: dict[str, dict[str, Any]]) -> dict[str, Any]:
    summary: dict[str, Any] = {"profile": profile, "ok": True, "problems": []}
    hashes: dict[str, int] = {}
    frames: dict[str, int] = {}
    stages: dict[str, str] = {}
    for name, sample in samples.items():
        net = sample["net"]
        perf = sample["perf"]
        fingerprint = sample["fingerprint"]
        hashes[name] = int(fingerprint["network"]["hash"])
        frames[name] = int(net["lockstep_next_frame"])
        stages[name] = str(net["stage"])
        if net["lockstep_last_desync_recovery_mode"] == "fatal-desync":
            summary["problems"].append(f"{name}: fatal desync")
        if int(net["lockstep_hash_mismatch_count"]) != 0:
            summary["problems"].append(
                f"{name}: hash mismatches={net['lockstep_hash_mismatch_count']}"
            )
        if net["join_barrier"]["active"]:
            summary["problems"].append(
                f"{name}: join barrier still active phase={net['join_barrier']['phase']}"
            )
        summary[name] = {
            "stage": net["stage"],
            "frame": net["lockstep_next_frame"],
            "delay": net["lockstep_input_delay_frames"],
            "rollback": net["lockstep_max_rollback_frames"],
            "rollbacks_per_second": net["lockstep_rollbacks_per_second"],
            "prediction_misses": net["lockstep_prediction_miss_count"],
            "skipped_inputs": net["lockstep_arbitrated_missing_input_count"],
            "confirmed_hash_frame": net["lockstep_last_confirmed_hash_frame"],
            "hash_mismatches": net["lockstep_hash_mismatch_count"],
            "network_hash": fingerprint["network"]["hash"],
            "multiplayer_sim_total_ms": perf["perf"]["multiplayer_sim_total_smoothed_ms"],
            "hash_ms": perf["perf"]["lockstep_hash_smoothed_ms"],
            "rollback_replay_ms": perf["perf"]["rollback_replay_smoothed_ms"],
        }

    if len(set(stages.values())) != 1:
        summary["problems"].append(f"stage mismatch: {stages}")
    if max(frames.values()) - min(frames.values()) > 2:
        summary["problems"].append(f"frame mismatch: {frames}")
    if len(set(hashes.values())) != 1:
        summary["problems"].append(f"network hash mismatch: {hashes}")
    summary["ok"] = len(summary["problems"]) == 0
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a running two-window lockstep session through splonksctl."
    )
    parser.add_argument("--host-control-port", type=int, default=DEFAULT_HOST_PORT)
    parser.add_argument("--peer-control-port", type=int, default=DEFAULT_PEER_PORT)
    parser.add_argument(
        "--profile",
        action="append",
        dest="profiles",
        help="fuzzer profile to run; repeatable. Defaults to same-house, tx-ca, tx-japan.",
    )
    parser.add_argument("--ready-timeout", type=float, default=20.0)
    parser.add_argument(
        "--json",
        action="store_true",
        help="print full JSON summary instead of one line per profile.",
    )
    args = parser.parse_args()

    host = Endpoint("host", args.host_control_port)
    peer = Endpoint("peer", args.peer_control_port)
    endpoints = [host, peer]
    profiles = tuple(args.profiles) if args.profiles else DEFAULT_PROFILES

    results: list[dict[str, Any]] = []
    try:
        wait_ready(endpoints, args.ready_timeout)
        for profile in profiles:
            apply_profile(endpoints, profile)
            wait_ready(endpoints, args.ready_timeout)
            run_action_sequence(host, peer)
            wait_ready(endpoints, args.ready_timeout)
            samples = {endpoint.name: collect(endpoint) for endpoint in endpoints}
            results.append(summarize_profile(profile, samples))
    except ControlError as exc:
        print(f"validate_lockstep_live: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps({"ok": all(result["ok"] for result in results), "results": results}, indent=2))
    else:
        for result in results:
            status = "ok" if result["ok"] else "FAIL"
            print(f"{result['profile']}: {status}")
            for name in ("host", "peer"):
                data = result.get(name, {})
                print(
                    "  "
                    f"{name}: stage={data.get('stage')} frame={data.get('frame')} "
                    f"delay={data.get('delay')} rollback={data.get('rollback')} "
                    f"rps={data.get('rollbacks_per_second'):.3f} "
                    f"misses={data.get('prediction_misses')} "
                    f"skipped={data.get('skipped_inputs')} "
                    f"hash_frame={data.get('confirmed_hash_frame')} "
                    f"hash_ms={data.get('hash_ms'):.3f} "
                    f"sim_ms={data.get('multiplayer_sim_total_ms'):.3f}"
                )
            for problem in result["problems"]:
                print(f"  problem: {problem}")
    return 0 if all(result["ok"] for result in results) else 1


if __name__ == "__main__":
    sys.exit(main())
