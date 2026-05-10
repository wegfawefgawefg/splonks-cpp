# Networking Protocol Cleanup Checklist

Review date: 2026-05-09

Purpose: remove the remaining refactor cruft in the networking layer and align
the code with the Terraria/tModLoader-style model we chose: broad message lanes,
coordinator-owned durable state, typed payloads, and no item-specific netcode
patches.

This is a focused cleanup checklist. The broader multiplayer definition of done
remains `docs/multiplayer_terraria_parity_checklist.md`.

## Rules

- Do not add item-named packet families for normal content behavior.
- Do not hide old gameplay-event-bus remnants behind wrapper names.
- Do not use anonymous parameter slots for typed gameplay/network payloads.
- Do not send one wide "all possible fields" action struct over the wire.
- Keep content rules in content/world-ops code. Network code should transport
  typed broad messages and replicated state, not know how specific items work.

## Checklist

- [ ] Split `src/network/net_lobby.cpp`.
  - Current smell: it still owns too many unrelated responsibilities: join and
    rejoin, retained player state, stage reload, packet pump, timeout cleanup,
    and session start.
  - Target shape: keep a small session orchestrator and move concrete domains
    into focused files such as join/rejoin, retained player state, stage sync,
    timeout/endpoint cleanup, and packet pumping.
  - Progress: host/peer UDP packet pump loops now live in
    `src/network/net_lobby_packet_pump.cpp`. Remaining split targets are
    join/rejoin, retained player state, and session lifecycle.
  - Progress: removed leftover one-line wrapper shims for join handlers,
    timeout cleanup, synced stage loading, and same-stage snapshot resync.

- [ ] Split `src/network/net_lobby_packets.cpp`.
  - Current smell: it is a mapper warehouse that converts every network payload
    shape to and from packet entries.
  - Target shape: split by broad protocol lane: tile, fluid, entity, player,
    run/stage, action, and presentation.
  - Packet mapping should stay mechanical. It should not encode content rules.
  - Progress: tile/fluid mapping now lives in
    `src/network/net_lobby_packets_tile.cpp`, and action request mapping now
    lives in `src/network/net_lobby_packets_action.cpp`. Remaining mapping
    lanes are entity, player, run/stage, and presentation.

- [ ] Split `src/network/net_event_apply.cpp`.
  - Current smell: it mixes apply logic for unrelated domains.
  - Target shape: split by the same broad lanes as packet mapping: tile, fluid,
    entity, player, run/stage, action, and presentation.
  - Apply code may call content/world-ops seams. It should not become another
    item-specific dispatch table.
  - Progress: tile/fluid apply logic now lives in
    `src/network/net_event_apply_tile.cpp`. Remaining apply lanes are entity,
    player, run/stage, action, and presentation.

- [ ] Rename networking "event" terminology.
  - Current smell: names like `NetEvent`, `*EventEntry`, and `*EventsPacket`
    are now confusing because the old internal gameplay event bus was removed.
  - Target naming: `NetMessage`, `*MessageEntry`, and `*MessagesPacket`, or an
    equivalent message/fact naming scheme.
  - This is not just cosmetic if it exposes old wrappers or bus-shaped code.
    Rename while deleting stale indirection.

- [x] Rename stale spawn helper names in content.
  - Current smell: content helpers such as `SpawnAndReplicateEntity...` now just
    call `world_ops::SpawnEntity`, so the names imply old replication behavior.
  - Known sites include `src/entities/box.cpp`, `src/entities/pot.cpp`, and
    `src/stage_break.cpp`.
  - Target names should describe local content intent, such as spawning at a
    top-left or center position, without saying "replicate".

- [x] Replace the internal wide `GameplayActionRequested` bag with typed actions.
  - Current smell: one struct contains optional ids, positions, velocities,
    damage fields, tool fields, and use-edge fields even though each action uses
    only a small subset.
  - Target shape: use a tagged union such as `std::variant` with explicit
    payload structs: `UseToolAction`, `UseHeldEntityAction`,
    `UseBackEntityAction`, `PickupEntityAction`, `DropEntityAction`,
    `ThrowEntityAction`, `PutHeldEntityOnBackAction`,
    `TakeOffBackEntityAction`, `InteractEntityAction`, `CollectEntityAction`,
    `PushEntityAction`, `BreakTileAction`, `DamageEntityAction`, and
    `HitEntityAction`.
  - Call sites should construct the specific action they mean. World-ops should
    dispatch on the typed action, not inspect a giant field bag.

- [x] Replace fixed-size network action request payloads with typed compact
  payloads.
  - Current smell: `ActionRequestEventEntry` reserves fields for every possible
    action and the protocol writes fixed-size structs. Most action packets waste
    most of their payload.
  - Target shape: use a small action request header plus a typed payload, or
    separate broad action message entries. Encode only the fields that action
    kind needs.
  - Match the Terraria/tModLoader principle: a broad message id selects a
    specific read/write shape; packets do not carry a uniform max-size action
    struct.
  - Add packet-size smoke checks so regressions are visible.
  - Done: `ActionRequestEvents` now uses variable-length encode/decode that
    writes only the fields for each action kind. The smoke test asserts the
    encoded packet is smaller than the old fixed-entry shape and round trips.

## Notes

The internal typed action union and the compact wire payload are separate but
related changes. The internal union removes ambiguous gameplay code. The compact
wire payload removes net-side waste and makes packet layout explicit. Both are
required; doing only a rename would leave the same architectural problem under
different names.
