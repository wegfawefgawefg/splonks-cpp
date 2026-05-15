#!/usr/bin/env python3
"""Live join/leave churn harness for Splonks lockstep multiplayer."""

from __future__ import annotations

import argparse
import json
import os
import random
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from churn_i3 import i3_arrange, i3_available, i3_msg, i3_wait_and_arrange


DEFAULT_BINARY = "build/splonks-cpp"
DEFAULT_NET_PORT = 39000
DEFAULT_HOST_CONTROL_PORT = 41000
DEFAULT_PEER_CONTROL_BASE = 41001
DEFAULT_MAX_PEERS = 7
DEFAULT_DURATION_S = 300.0


class ControlError(RuntimeError):
    pass


@dataclass
class ProcessSlot:
    label: str
    control_port: int
    proc: subprocess.Popen[Any]
    log_path: Path
    started_at: float
    deadline_at: float | None = None
    generation: int = 0
    preferred_player_id: int | None = None


@dataclass
class FrameWatch:
    frame: int | None = None
    changed_at: float = 0.0


@dataclass
class BarrierWatch:
    signature: tuple[Any, ...] | None = None
    changed_at: float = 0.0
    active_since: float | None = None


def now_s() -> float:
    return time.monotonic()


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def control(port: int, command: str, timeout: float = 0.75) -> dict[str, Any]:
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
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
        raise ControlError(f"bad JSON from {port} for `{command}`: {raw}") from exc
    if not parsed.get("ok", False):
        raise ControlError(f"{port} `{command}` failed: {parsed}")
    return parsed


def try_control(port: int, command: str, timeout: float = 0.75) -> dict[str, Any] | None:
    try:
        return control(port, command, timeout)
    except (OSError, TimeoutError, ControlError):
        return None


def connected_player_ids(players_response: dict[str, Any]) -> list[int]:
    return sorted(
        int(slot["player_id"])
        for slot in players_response.get("players", [])
        if slot.get("connected") and int(slot.get("player_id", -1)) >= 0
    )


def local_player_ids(players_response: dict[str, Any]) -> list[int]:
    return sorted(
        int(slot["player_id"])
        for slot in players_response.get("players", [])
        if slot.get("connected")
        and slot.get("connection") == "local"
        and int(slot.get("player_id", -1)) >= 0
    )


def barrier_signature(net_response: dict[str, Any]) -> tuple[Any, ...]:
    barrier = net_response.get("join_barrier", {})
    resync = net_response.get("snapshot_resync", {})
    return (
        barrier.get("active"),
        barrier.get("phase"),
        barrier.get("id"),
        barrier.get("active_peer_id"),
        barrier.get("queued_peers"),
        barrier.get("transfer_id"),
        barrier.get("chunks_done"),
        barrier.get("chunk_count"),
        barrier.get("bytes_done"),
        barrier.get("total_bytes"),
        resync.get("active_transfer_id"),
        resync.get("received_chunks"),
        resync.get("chunk_count"),
    )


def is_barrier_active(net_response: dict[str, Any]) -> bool:
    barrier = net_response.get("join_barrier", {})
    resync = net_response.get("snapshot_resync", {})
    return bool(barrier.get("active")) or bool(resync.get("active_transfer_id"))


def make_log_dir(repo_root: Path) -> Path:
    log_dir = repo_root / "logs" / "lockstep_churn"
    log_dir.mkdir(parents=True, exist_ok=True)
    return log_dir


def launch_process(
    *,
    label: str,
    cmd: list[str],
    repo_root: Path,
    log_dir: Path,
    generation: int,
) -> ProcessSlot:
    log_path = log_dir / f"{label}_g{generation}_{int(time.time())}.log"
    log_file = log_path.open("wb")
    proc = subprocess.Popen(
        cmd,
        cwd=repo_root,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        start_new_session=True,
        env=os.environ.copy(),
    )
    log_file.close()
    return ProcessSlot(
        label=label,
        control_port=control_port_from_cmd(cmd),
        proc=proc,
        log_path=log_path,
        started_at=now_s(),
        generation=generation,
    )


def make_peer_cmd(
    binary: Path,
    net_port: int,
    control_port: int,
    preferred_player_id: int | None,
    random_input: bool,
) -> list[str]:
    cmd = [str(binary), "--multiplayer-join", "127.0.0.1", str(net_port)]
    cmd += ["--debug-control-port", str(control_port)]
    if preferred_player_id is not None:
        cmd += ["--preferred-player-ids", str(preferred_player_id)]
    if random_input:
        cmd.append("--debug-random-primary-input")
    return cmd


def control_port_from_cmd(cmd: list[str]) -> int:
    for index, token in enumerate(cmd):
        if token == "--debug-control-port" and index + 1 < len(cmd):
            return int(cmd[index + 1])
    raise ValueError(f"missing --debug-control-port in {cmd}")


def terminate_process(slot: ProcessSlot, hard: bool, grace_s: float = 1.0) -> str:
    if slot.proc.poll() is not None:
        return "already-exited"
    if hard:
        os.killpg(slot.proc.pid, signal.SIGKILL)
        return "sigkill"
    try_control(slot.control_port, "quit", timeout=0.25)
    try:
        slot.proc.wait(timeout=grace_s)
        return "quit"
    except subprocess.TimeoutExpired:
        pass
    slot.proc.terminate()
    try:
        slot.proc.wait(timeout=grace_s)
        return "sigterm"
    except subprocess.TimeoutExpired:
        os.killpg(slot.proc.pid, signal.SIGKILL)
        return "sigterm-then-sigkill"


def kill_existing(binary: Path, net_port: int, control_ports: list[int]) -> None:
    for control_port in control_ports:
        for pattern in (
            f"{binary} --multiplayer-host {net_port} --debug-control-port {control_port}",
            f"{binary} --multiplayer-join 127.0.0.1 {net_port} --debug-control-port {control_port}",
        ):
            subprocess.run(["pkill", "-f", pattern], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def emit(event_log: Path, event: dict[str, Any]) -> None:
    event.setdefault("time", time.time())
    with event_log.open("a", encoding="utf-8") as file:
        file.write(json.dumps(event, sort_keys=True) + "\n")
    kind = event.get("event", "event")
    detail = event.get("detail") or event.get("label") or ""
    print(f"[{time.strftime('%H:%M:%S')}] {kind} {detail}".rstrip(), flush=True)


def sample(
    *,
    host: ProcessSlot,
    peers: dict[int, ProcessSlot],
    frame_watch: dict[str, FrameWatch],
    barrier_watch: BarrierWatch,
    event_log: Path,
    stall_s: float,
    barrier_stall_s: float,
) -> None:
    endpoints = [host, *peers.values()]
    host_net = try_control(host.control_port, "net")
    host_players = try_control(host.control_port, "players")
    if host_net is None or host_players is None:
        if now_s() - host.started_at > 3.0:
            emit(event_log, {"event": "problem", "detail": "host debug-control unreachable"})
        return

    host_ids = connected_player_ids(host_players)
    active_barrier = is_barrier_active(host_net)
    signature = barrier_signature(host_net)
    now = now_s()
    if signature != barrier_watch.signature:
        barrier_watch.signature = signature
        barrier_watch.changed_at = now
        if active_barrier and barrier_watch.active_since is None:
            barrier_watch.active_since = now
    elif active_barrier and now - barrier_watch.changed_at > barrier_stall_s:
        detail = f"join/resync barrier stalled {now - barrier_watch.changed_at:.1f}s"
        emit(event_log, {"event": "problem", "detail": detail, "host_net": host_net})
        barrier_watch.changed_at = now
    if not active_barrier:
        barrier_watch.active_since = None
    for endpoint in endpoints:
        net = try_control(endpoint.control_port, "net")
        players = try_control(endpoint.control_port, "players")
        if net is None or players is None:
            if endpoint is not host and endpoint.proc.poll() is not None:
                continue
            if now - endpoint.started_at > 3.0:
                emit(event_log, {"event": "problem", "detail": f"{endpoint.label} unreachable"})
            continue
        if int(net.get("lockstep_hash_mismatch_count", 0)) != 0:
            emit(event_log, {"event": "problem", "detail": f"{endpoint.label} hash mismatch", "net": net})
        if net.get("lockstep_last_desync_recovery_mode") == "fatal-desync":
            emit(event_log, {"event": "problem", "detail": f"{endpoint.label} fatal desync", "net": net})

        frame = int(net.get("lockstep_next_frame", 0))
        watch = frame_watch.setdefault(endpoint.label, FrameWatch(frame=frame, changed_at=now))
        if frame != watch.frame:
            watch.frame = frame
            watch.changed_at = now
        elif not active_barrier and now - watch.changed_at > stall_s:
            emit(event_log, {"event": "problem", "detail": f"{endpoint.label} frame stalled"})
            watch.changed_at = now

        if endpoint is not host and not active_barrier and not is_barrier_active(net):
            ids = connected_player_ids(players)
            local_ids = local_player_ids(players)
            if local_ids and all(player_id in host_ids for player_id in local_ids) and ids != host_ids:
                detail = f"{endpoint.label} topology differs host={host_ids} peer={ids}"
                emit(event_log, {"event": "problem", "detail": detail})


def update_retained_ids(peers: dict[int, ProcessSlot], retained_ids: list[int | None]) -> None:
    for slot_index, peer in peers.items():
        players = try_control(peer.control_port, "players", timeout=0.25)
        if players is None:
            continue
        ids = local_player_ids(players)
        if ids:
            retained_ids[slot_index] = ids[0]


def session_is_playing(host: ProcessSlot, peers: dict[int, ProcessSlot]) -> bool:
    endpoints = [host, *peers.values()]
    for endpoint in endpoints:
        net = try_control(endpoint.control_port, "net", timeout=0.35)
        if net is None or is_barrier_active(net):
            return False
    return True


def peer_for_player_id(peers: dict[int, ProcessSlot], player_id: int) -> tuple[int, ProcessSlot] | None:
    for slot_index, peer in peers.items():
        players = try_control(peer.control_port, "players", timeout=0.2)
        if players is None:
            continue
        if player_id in local_player_ids(players):
            return slot_index, peer
    return None


def maybe_drop_during_sync(
    *,
    host: ProcessSlot,
    peers: dict[int, ProcessSlot],
    rng: random.Random,
    percent: float,
    hard_drop_percent: float,
    event_log: Path,
) -> bool:
    if percent <= 0.0 or not peers:
        return False
    host_net = try_control(host.control_port, "net", timeout=0.25)
    if host_net is None or not is_barrier_active(host_net):
        return False
    if rng.random() >= percent / 100.0:
        return False

    barrier = host_net.get("join_barrier", {})
    active_peer_id = int(barrier.get("active_peer_id", -1))
    chosen: tuple[int, ProcessSlot] | None = None
    if active_peer_id >= 0:
        chosen = peer_for_player_id(peers, active_peer_id)
    if chosen is None:
        chosen = rng.choice(list(peers.items()))

    slot_index, peer = chosen
    hard = rng.random() < hard_drop_percent / 100.0
    mode = terminate_process(peer, hard=hard)
    emit(event_log, {
        "event": "peer-drop-during-sync",
        "label": peer.label,
        "mode": mode,
        "active_peer_id": active_peer_id,
    })
    del peers[slot_index]
    return True


def random_input_tick(
    peers: dict[int, ProcessSlot],
    host: ProcessSlot,
    rng: random.Random,
    chance: float,
    include_host: bool,
) -> None:
    buttons = [
        ("left",), ("right",), ("left", "jump"), ("right", "jump"), ("jump",),
        ("pickup",), ("attack",), ("rope",), ("bomb",),
    ]
    endpoints = [host, *peers.values()] if include_host else list(peers.values())
    for endpoint in endpoints:
        if rng.random() > chance:
            continue
        players = try_control(endpoint.control_port, "players", timeout=0.25)
        if players is None:
            continue
        locals_ = local_player_ids(players)
        if not locals_:
            continue
        player_id = rng.choice(locals_)
        frames = rng.randint(4, 35)
        chosen = rng.choice(buttons)
        try_control(endpoint.control_port, f"input player {player_id} {frames} {' '.join(chosen)}", timeout=0.25)


def main() -> int:
    parser = argparse.ArgumentParser(description="Random live join/leave churn test.")
    parser.add_argument("--repo-root", type=Path, default=repo_root_from_script())
    parser.add_argument("--binary", type=Path, default=Path(DEFAULT_BINARY))
    parser.add_argument("--net-port", type=int, default=DEFAULT_NET_PORT)
    parser.add_argument("--host-control-port", type=int, default=DEFAULT_HOST_CONTROL_PORT)
    parser.add_argument("--peer-control-base", type=int, default=DEFAULT_PEER_CONTROL_BASE)
    parser.add_argument("--max-peers", type=int, default=DEFAULT_MAX_PEERS)
    parser.add_argument("--mode", choices=("adversarial", "cohort"), default="adversarial")
    parser.add_argument("--cohort-size", type=int, default=0)
    parser.add_argument("--play-window-s", type=float, default=8.0)
    parser.add_argument("--duration-s", type=float, default=DEFAULT_DURATION_S)
    parser.add_argument("--min-live-s", type=float, default=1.0)
    parser.add_argument("--max-live-s", type=float, default=30.0)
    parser.add_argument("--spawn-min-s", type=float, default=0.2)
    parser.add_argument("--spawn-max-s", type=float, default=4.0)
    parser.add_argument("--hard-drop-percent", type=float, default=35.0)
    parser.add_argument("--sync-drop-percent", type=float, default=0.0)
    parser.add_argument("--rejoin-percent", type=float, default=50.0)
    parser.add_argument("--input-chance", type=float, default=0.18)
    parser.add_argument("--random-host-input", action="store_true")
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--kill-existing", action="store_true")
    parser.add_argument("--no-random-peer-input", action="store_true")
    parser.add_argument("--i3-workspace", default="2")
    parser.add_argument("--i3-output", default="DisplayPort-0")
    parser.add_argument("--i3-cols", type=int, default=2)
    parser.add_argument("--i3-rows", type=int, default=4)
    parser.add_argument("--no-i3-arrange", action="store_true")
    parser.add_argument("--stall-s", type=float, default=8.0)
    parser.add_argument("--barrier-stall-s", type=float, default=15.0)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    binary = args.binary if args.binary.is_absolute() else repo_root / args.binary
    log_dir = make_log_dir(repo_root)
    event_log = log_dir / f"churn_{int(time.time())}.jsonl"
    rng = random.Random(args.seed)

    if args.kill_existing:
        ports = [args.host_control_port] + [args.peer_control_base + index for index in range(args.max_peers)]
        kill_existing(binary, args.net_port, ports)
        time.sleep(0.25)
    if not args.no_i3_arrange and i3_available():
        i3_msg(f"workspace number {args.i3_workspace}")
        if args.i3_output:
            i3_msg(f"move workspace to output {args.i3_output}")

    host_cmd = [str(binary), "--multiplayer-host", str(args.net_port), "--debug-control-port", str(args.host_control_port)]
    host = launch_process(label="host", cmd=host_cmd, repo_root=repo_root, log_dir=log_dir, generation=0)
    peers: dict[int, ProcessSlot] = {}
    generations = [0 for _ in range(args.max_peers)]
    retained_ids: list[int | None] = [None for _ in range(args.max_peers)]
    frame_watch: dict[str, FrameWatch] = {}
    barrier_watch = BarrierWatch(changed_at=now_s())
    next_spawn_at = now_s() + 0.4
    next_cohort_drop_at: float | None = None
    cohort_size = args.cohort_size if args.cohort_size > 0 else args.max_peers
    cohort_size = max(0, min(cohort_size, args.max_peers))
    end_at = now_s() + args.duration_s
    emit(event_log, {"event": "host-started", "label": "host", "port": args.host_control_port})

    try:
        while now_s() < end_at:
            if host.proc.poll() is not None:
                emit(event_log, {"event": "fatal", "detail": f"host exited rc={host.proc.returncode}"})
                return 2

            now = now_s()
            update_retained_ids(peers, retained_ids)
            for slot_index, peer in list(peers.items()):
                if peer.proc.poll() is not None:
                    emit(event_log, {
                        "event": "peer-exited", "label": peer.label,
                        "rc": peer.proc.returncode, "log": str(peer.log_path),
                    })
                    del peers[slot_index]
                    continue
                if args.mode == "adversarial" and peer.deadline_at is not None and now >= peer.deadline_at:
                    hard = rng.random() < args.hard_drop_percent / 100.0
                    mode = terminate_process(peer, hard=hard)
                    emit(event_log, {"event": "peer-drop", "label": peer.label, "mode": mode})
                    del peers[slot_index]

            should_spawn = now >= next_spawn_at and len(peers) < args.max_peers
            if args.mode == "cohort":
                should_spawn = len(peers) < cohort_size
            if should_spawn:
                free_slots = [index for index in range(args.max_peers) if index not in peers]
                if free_slots:
                    slot_index = free_slots[0] if args.mode == "cohort" else rng.choice(free_slots)
                    generations[slot_index] += 1
                    control_port = args.peer_control_base + slot_index
                    label = f"peer{slot_index + 2}"
                    preferred = retained_ids[slot_index]
                    if preferred is not None and rng.random() >= args.rejoin_percent / 100.0:
                        preferred = None
                    cmd = make_peer_cmd(binary, args.net_port, control_port, preferred, not args.no_random_peer_input)
                    peer = launch_process(
                        label=label,
                        cmd=cmd,
                        repo_root=repo_root,
                        log_dir=log_dir,
                        generation=generations[slot_index],
                    )
                    peer.preferred_player_id = preferred
                    if args.mode == "adversarial":
                        peer.deadline_at = now + rng.uniform(args.min_live_s, args.max_live_s)
                    peers[slot_index] = peer
                    live_s = None if peer.deadline_at is None else round(peer.deadline_at - now, 2)
                    emit(event_log, {
                        "event": "peer-started", "label": label, "port": control_port,
                        "preferred_player_id": preferred, "live_s": live_s,
                    })
                    if not args.no_i3_arrange:
                        time.sleep(0.15)
                        expected_windows = 1 + len(peers)
                        i3_wait_and_arrange(
                            args.i3_workspace,
                            args.i3_output,
                            cols=args.i3_cols,
                            rows=args.i3_rows,
                            expected_windows=expected_windows,
                            timeout_s=2.0,
                        )
                if args.mode == "adversarial":
                    next_spawn_at = now + rng.uniform(args.spawn_min_s, args.spawn_max_s)

            if maybe_drop_during_sync(
                host=host,
                peers=peers,
                rng=rng,
                percent=args.sync_drop_percent,
                hard_drop_percent=args.hard_drop_percent,
                event_log=event_log,
            ):
                if not args.no_i3_arrange:
                    i3_arrange(args.i3_workspace, args.i3_output, cols=args.i3_cols, rows=args.i3_rows)
                time.sleep(0.25)
                continue

            if args.mode == "cohort" and len(peers) == cohort_size and session_is_playing(host, peers):
                if next_cohort_drop_at is None:
                    next_cohort_drop_at = now + args.play_window_s
                    emit(event_log, {"event": "cohort-playing", "detail": f"drop in {args.play_window_s:.1f}s"})
                elif now >= next_cohort_drop_at and peers:
                    slot_index = rng.choice(list(peers.keys()))
                    peer = peers[slot_index]
                    hard = rng.random() < args.hard_drop_percent / 100.0
                    mode = terminate_process(peer, hard=hard)
                    emit(event_log, {"event": "peer-drop", "label": peer.label, "mode": mode})
                    del peers[slot_index]
                    next_cohort_drop_at = None
            elif args.mode == "cohort":
                next_cohort_drop_at = None

            sample(
                host=host,
                peers=peers,
                frame_watch=frame_watch,
                barrier_watch=barrier_watch,
                event_log=event_log,
                stall_s=args.stall_s,
                barrier_stall_s=args.barrier_stall_s,
            )
            random_input_tick(peers, host, rng, args.input_chance, args.random_host_input)
            time.sleep(0.25)
    except KeyboardInterrupt:
        emit(event_log, {"event": "interrupted"})
    finally:
        for peer in list(peers.values()):
            terminate_process(peer, hard=False, grace_s=0.5)
        terminate_process(host, hard=False, grace_s=0.5)
        emit(event_log, {"event": "finished", "log": str(event_log)})
    return 0


if __name__ == "__main__":
    sys.exit(main())
