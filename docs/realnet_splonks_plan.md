# Realnet Splonks Plan

Realnet is the next networking phase for Splonks and Gubsy. The goal is public
internet multiplayer that works for normal players without manual router
configuration.

Splonks is the first real game consumer. Gubsy owns discovery, rendezvous,
connection cascade, and later relay/Steam backends. Splonks owns its lockstep
game protocol, player topology, rollback, stage synchronization, and gameplay
UI.

## Current State

Splonks currently has:

1. Direct host/join by IP and UDP port.
2. Public room publishing through `gubsy-roomd`.
3. Public room browser joins through the in-game lobby.
4. Lockstep/rollback gameplay once connected.
5. Join-in-progress snapshot catchup.
6. Desync replay logging and replay comparison tools.

This is now good enough for same-machine and LAN testing. It is not enough for
normal public internet play because `gubsy-roomd` is a directory service, not a
relay, and the current path still needs the host UDP port to be reachable.

## Target

The target user experience:

1. Host starts a public Splonks room.
2. Room appears in the browser through `gubsy-roomd`.
3. Remote player selects the room.
4. Splonks/Gubsy tries the connection cascade.
5. If LAN/direct works, use it.
6. If direct internet fails, try UDP NAT punch-through.
7. Later, if punch-through fails, use relay.
8. Later, Steam builds can use Steam lobby/transport through the same conceptual
   flow.

The player should not need to know about port forwarding.

## Game Boundary

Splonks should not implement NAT traversal itself.

Splonks should provide:

1. Game/session metadata.
2. Local player count and profile metadata.
3. Compatibility requirements.
4. Lockstep packet send/receive callbacks.
5. Lobby and loading UI for connection phases.

Gubsy should provide:

1. Room discovery.
2. Join authorization.
3. Connection candidate list.
4. Direct/LAN/public endpoint attempts.
5. UDP rendezvous and NAT punch-through.
6. Relay fallback later.
7. Steam transport later.
8. A connected packet transport abstraction.

Splonks should treat the resulting transport as a packet pipe. The existing
lockstep protocol should sit above it.

## Authority Mode

Splonks should use Gubsy's `PlayerHost` authority mode first.

```text
PlayerHost:
  a normal Splonks client hosts the room
  joiners connect to that host
  host owns run start, player topology, and snapshot catchup
```

Later, Splonks may add a headless dedicated server:

```text
DedicatedServer:
  splonks-server runs without menu UI
  server publishes a room through Gubsy
  players connect to the server instead of a player host
```

That should not require a different browser flow. It should be another room
authority mode.

## Connection Cascade In Splonks UI

The Splonks browser should eventually stop showing room entries as raw
`ip:port` joins. A room should be shown as a game session with connection
status.

Recommended phases:

```text
Resolving room
Checking compatibility
Trying local connection
Trying LAN direct
Trying public direct
Trying NAT traversal
Using relay
Connected
Failed
```

The lobby status area should show the selected transport after connection:

```text
Currently Public Hosting via gubsy-roomd
Connected via NAT punch
Connected via relay
Connected via Steam
```

Failure copy should be specific:

```text
No compatible room found.
Room is full.
Version mismatch.
Direct connection failed.
NAT traversal failed.
Relay unavailable.
Host stopped responding.
```

## Browser Behavior

The browser should show:

1. Room name.
2. Host or server display name.
3. Room phase: lobby or in-game.
4. Player count.
5. Compatibility.
6. Candidate/transport hints when useful.
7. Ping/age when available.

The browser should not promise that a room is joinable merely because it exists
in the directory. It should show "joinable" only after compatibility and join
attempt policy are known.

## Direct IP Join

Keep direct IP join as a developer and power-user path.

Direct IP join should remain useful for:

1. Same-machine testing.
2. LAN testing.
3. Explicit server IP testing.
4. Debugging transport regressions.

But direct IP should not be the normal public internet UX.

## Lockstep Compatibility

Realnet should not change the lockstep protocol's meaning.

After Gubsy returns a connected transport, Splonks should still:

1. Establish host/client roles.
2. Synchronize player topology.
3. Run the join barrier for late joiners.
4. Send authoritative snapshot catchup where needed.
5. Maintain lockstep transport every fixed tick.
6. Use rollback/desync tooling as today.

The connection cascade is below this layer. It decides how packets move, not
what the packets mean.

## Join-In-Progress

Join-in-progress should continue to be host-owned:

1. Client joins the room through Realnet.
2. Host accepts the join attempt.
3. Host sends full player topology and current game snapshot.
4. Client completes catchup.
5. Client enters lobby or active play depending on host phase.

Realnet does not decide where a player spawns or which snapshot fields are
authoritative. Splonks does.

## Dedicated Server Future

A future `splonks-server` should:

1. Run without SDL window/menu UI.
2. Load game content and config.
3. Publish one or more rooms through Gubsy.
4. Accept clients through the same Realnet transport interface.
5. Own simulation authority or lockstep host duties, depending on the chosen
   Splonks server model.

This is useful for VPS-hosted persistent or scheduled games. It is separate from
`gubsy-roomd`; the room daemon should not become a Splonks game server.

## Steam Future

Steam should fit the same shape:

1. Steam lobby discovery maps to a room/session listing.
2. SteamNetworkingSockets maps to a transport backend.
3. Steam relay is a transport implementation detail.
4. Splonks still sees connected peers and opaque packet send/receive.

Steam-only rooms may be Steam-only. Cross-play rooms should explicitly expose a
non-Steam path or Gubsy relay path.

## Instrumentation

Splonks should expose Realnet diagnostics in developer builds:

1. Current room ID/code.
2. Host/client/dedicated authority mode.
3. Selected transport.
4. Candidate attempts and timing.
5. Public observed endpoint.
6. NAT punch success/failure.
7. Relay fallback reason.
8. Connection failure reason.
9. Lockstep frame, stage instance, and desync replay path after connection.

This should be visible in logs and reachable through the existing debug control
server where practical.

## Validation Plan

Desktop-first validation:

1. Same-machine host/browser join still works.
2. LAN host/browser join still works.
3. LAN direct IP join still works.
4. Cross-network browser join attempts NAT traversal.
5. Failed NAT traversal reports a useful reason.
6. Successful NAT traversal can start, join in progress, restart, leave, and
   stage transition without desync.

Use `docs/realnet_lan_validation.md` for the two-machine LAN proof. The
launcher supports `--host-only` and `--client-only` so the host can run roomd
and one Splonks window while the second machine runs only the joining client.
Before the two-machine run, `scripts/validate_gubsy_roomd_live.sh
--lan-interface` verifies the same browser path against roomd through the host's
LAN-facing address. The final two-machine result should be captured in
`logs/realnet_lan_verdict.json` from `docs/realnet_lan_verdict_template.json`
and audited with `scripts/summarize_realnet_lan_validation.py`. The full
foundation gate is `scripts/audit_realnet_foundation.sh`, which reruns the
focused local evidence and then requires that verdict.

Later validation:

1. Relay fallback works when direct/punch fails.
2. Steam build uses Steam transport without changing Splonks lockstep code.
3. Dedicated server room appears and is joinable through the same browser.

## Milestones

### Milestone 1: Adopt Realnet Metadata

1. Add Splonks room metadata for authority mode and protocol version.
2. Keep current room browser working.
3. Show connection phase and selected transport in UI/logs.

### Milestone 2: Route Browser Join Through Gubsy Cascade

1. Replace raw browser endpoint join with `ConnectToRoom` style flow.
2. Preserve direct IP as a separate screen.
3. Keep lockstep startup code above the transport boundary.

### Milestone 3: NAT Punch Smoke Testing

1. Use `gubsy-roomd` UDP rendezvous.
2. Test host on home LAN and client on phone hotspot or other network.
3. Record success/failure reasons.
4. Capture desync replay logs for gameplay regressions only.

### Milestone 4: Public Internet Playtest

1. Run roomd on reachable VPS.
2. Join across unrelated networks without port forwarding.
3. Measure latency, rollback behavior, and stage transition stability.
4. Decide whether relay must be next before Steam.

### Milestone 5: Relay Or Steam Backend

1. Add Gubsy relay if non-Steam public play needs guaranteed connectivity.
2. Add Steam transport when Steamworks access is available.
3. Keep Splonks protocol unchanged across backend choices.

## Open Questions

1. Should Splonks expose "allow relay" as a player setting, or should it be a
   build/server policy?
2. How much connection detail belongs in normal UI versus developer overlay?
3. What is the first useful `splonks-server` mode: lockstep host, authoritative
   server, or headless player-host equivalent?
4. Should public rooms advertise estimated latency before join, or only after a
   join attempt starts?

## Immediate Next Step

Validate the current direct-candidate foundation on a real LAN with
`docs/realnet_lan_validation.md`, then implement the Gubsy UDP rendezvous path
for cross-network joins without router port forwarding.

## Current NAT Punch Foundation

The first forced NAT-punch path is implemented behind developer environment
flags:

```bash
SPLONKS_REALNET_FORCE_NAT_PUNCH=1 ./scripts/validate_gubsy_roomd_live.sh
```

This starts local `gubsy-roomd`, hosts a public room, joins through the room
browser, and forces the browser join to use the Realnet UDP rendezvous path
rather than the advertised direct candidate. The game sends Realnet rendezvous
traffic from the same UDP socket used for gameplay, so the NAT mapping being
punched is the mapping that subsequent lockstep packets use.

Optional override:

```bash
SPLONKS_REALNET_RENDEZVOUS_PORT=8791
```

Direct-failure fallback can be validated locally by advertising an unroutable
direct endpoint and shortening the developer timeout:

```bash
SPLONKS_ADVERTISE_HOST=203.0.113.1 \
SPLONKS_REALNET_DIRECT_TIMEOUT_MS=1 \
./scripts/validate_gubsy_roomd_live.sh
```

When unset, Splonks reads the rendezvous UDP endpoint from Gubsy room-server
capabilities. If the server does not advertise capabilities, it falls back to
`HTTP_PORT + 1`, matching the current `gubsy-roomd` default.

Normal browser joins use a connection cascade:

1. Use direct UDP first for public endpoints and private endpoints that look
   local to the client.
2. Skip direct for private IPv4 endpoints that are not on the client's local
   `/24`, because those are usually another player's LAN address and cannot be
   reached from a phone hotspot or public internet client.
3. Use Realnet UDP rendezvous/NAT punch when punch credentials are present.
4. Fall back to direct-timeout failure handling when no Realnet credentials are
   available.

Current proof:

1. Local forced-punch smoke passes on one machine.
2. Direct-failure fallback smoke passes on one machine.
3. Same-LAN proof can use the same env flag with
   `./scripts/validate_gubsy_roomd_live.sh --lan-interface`.
4. Two-machine same-LAN forced-punch proof passes with desktop host and laptop
   client using the headless commands in `docs/realnet_lan_validation.md`.
5. Real NAT traversal passes with desktop host on home internet, laptop client
   on phone hotspot, and Tokyo VPS roomd as the public rendezvous coordinator.
   The unforced path skips the desktop's private LAN endpoint on the hotspot
   client and connects through Realnet NAT punch.
