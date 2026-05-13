# Current Multiplayer Cleanup Status

Review date: 2026-05-10

Scope: current uncommitted multiplayer/world-ops work.

Source of truth: `docs/multiplayer_terraria_parity_checklist.md`.

This file is no longer a slice-by-slice migration log. The old working checklist
became stale because completed migration notes and real remaining work were mixed
together. Keep this file short; update the parity checklist for active
multiplayer gates.

## Completed Cleanup

- [x] Removed the old internal gameplay mutation queue.
- [x] Routed durable gameplay mutations through canonical `world_ops` seams.
- [x] Replaced peer-authored durable mutations with generic host action
  requests and host-authored result messages.
- [x] Added typed action payloads instead of a wide optional-field action bag.
- [x] Added compact variable-length action request packets.
- [x] Split networking protocol files by broad lane.
- [x] Standardized network protocol terminology around messages.
- [x] Split networking god files:
  - `src/network/net_lobby.cpp`
  - `src/network/net_lobby_packets.cpp`
  - `src/network/net_message_apply.cpp`
- [x] Added fake-transport/network smoke coverage for protocol apply, action
  requests, packet encode/decode, frame stepping, stage transition, and
  respawn-while-held cases.

## Active Follow-Ups

Track these in `docs/multiplayer_terraria_parity_checklist.md`, not by adding a
second competing checklist here:

- Debug/admin command lane for multiplayer-safe editor/admin actions.
- Pres coverage audit for cosmetic feedback such as sounds, particles,
  weapon effects, explosions, and teleports.
- Expanded headless/fake-transport scenarios for multi-peer runs, shops, water,
  bombs/explosions, and fuzzer presets.
- Prediction/interpolation polish after convergence is proven.
- Review local editor/playtest files before committing if they appear in
  `git status`.

## Verification Commands

- `cmake --build build --target splonks-cpp -j 8`
- `build/splonks-cpp --check-network-protocol-smoke`
- `build/splonks-cpp --check-network-action-smoke`
- `build/splonks-cpp --check-network-packet-smoke`
- `build/splonks-cpp --check-network-frame-smoke`
- `git diff --check`
