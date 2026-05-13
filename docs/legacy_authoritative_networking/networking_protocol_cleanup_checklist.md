# Networking Protocol Cleanup Completion

Review date: 2026-05-10

Purpose: record the networking cleanup work that removed refactor cruft and
aligned the code with the Terraria/tModLoader-style model we chose: broad
message lanes, host-owned durable state, typed payloads, and no
item-specific netcode patches.

The broader multiplayer definition of done remains
`docs/multiplayer_terraria_parity_checklist.md`.

## Rules

- Do not add item-named packet families for normal content behavior.
- Do not hide old gameplay-message-bus remnants behind wrapper names.
- Do not use anonymous parameter slots for typed gameplay/network payloads.
- Do not send one wide "all possible fields" action struct over the wire.
- Keep content rules in content/world-ops code. Network code should transport
  typed broad messages and replicated state, not know how specific items work.

## Completed Work

- [x] Split `src/network/net_lobby.cpp`.
  - The root file is now only the per-frame networking orchestrator and
    transport query helpers.
  - Host/peer UDP packet pump loops live in
    `src/network/net_lobby_packet_pump.cpp`.
  - Join/rejoin/leave handling lives in `src/network/net_lobby_join.cpp`.
  - Retained disconnected-player body and reconnect state lives in
    `src/network/net_lobby_retained_player.cpp`.
  - Session start/stop lives in `src/network/net_lobby_session.cpp`.
  - Stage sync/reload lives in `src/network/net_lobby_stage.cpp`.
  - Endpoint timeout cleanup lives in `src/network/net_lobby_endpoint.cpp`.
  - Respawn/revive player lifecycle lives in
    `src/network/net_lobby_player_lifecycle.cpp`.
  - Leftover one-line wrapper shims for join handlers, timeout cleanup, synced
    stage loading, and same-stage snapshot resync were removed.

- [x] Split `src/network/net_lobby_packets.cpp`.
  - The root file is now only the shared packet-send helper.
  - Tile/fluid, action, ent, player/run, and pres mapping live in
    focused `net_lobby_packets_*.cpp` files.
  - Packet mapping stays mechanical and does not encode content rules.

- [x] Split `src/network/net_message_apply.cpp`.
  - The root file is now the ordered-message dispatcher and shared ordering
    helpers.
  - Tile/fluid, ent, player, run/stage, and pres apply logic live in
    focused `net_message_apply_*.cpp` files.
  - Apply code calls content/world-ops seams instead of becoming an item-specific
    dispatch table.

- [x] Standardized networking protocol terminology.
  - Protocol structs/files now use message terminology consistently:
    `NetMessage`, message entries, message packets, and message logs.
  - This distinguishes the network protocol from the removed internal gameplay
    mutation bus.

- [x] Renamed stale spawn helper names in content.
  - Helpers that now call `world_ops::SpawnEnt` no longer imply that content
    code directly owns replication.
  - Known sites covered include `src/ents/box.cpp`, `src/ents/pot.cpp`,
    and `src/stage_break.cpp`.

- [x] Replaced the internal wide action bag with typed actions.
  - Call sites construct specific action payloads such as `UseToolAction`,
    `UseHeldEntAction`, `PickupEntAction`, `ThrowEntAction`,
    `BreakTileAction`, `DamageEntAction`, and `HitEntAction`.
  - World-ops dispatches on typed action payloads instead of inspecting a giant
    optional-field bag.

- [x] Replaced fixed-size network action request payloads with typed compact
  payloads.
  - Action request messages now use variable-length encode/decode and write only
    the fields required by each action kind.
  - Packet smoke asserts the encoded packet is smaller than the old fixed-entry
    shape and round trips correctly.

## Verification

- [x] `cmake --build build --target splonks-cpp -j 8`
- [x] `build/splonks-cpp --check-network-protocol-smoke`
- [x] `build/splonks-cpp --check-network-action-smoke`
- [x] `build/splonks-cpp --check-network-packet-smoke`
- [x] `build/splonks-cpp --check-network-frame-smoke`
- [x] `git diff --check`

## Notes

The internal typed action union and the compact wire payload are separate but
related changes. The internal union removes ambiguous gameplay code. The compact
wire payload removes net-side waste and makes packet layout explicit.
