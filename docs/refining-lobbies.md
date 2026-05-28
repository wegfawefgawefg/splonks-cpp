# Refining Lobbies

## Goal

Make the Gubsy/Splonks lobby flow understandable, reliable, and useful for real
host/join play without requiring ImGui debug networking menus.

The current direct and browser flow works enough to host and join from two
clients, but the menu semantics, labels, host state, room browser behavior, and
joined-client experience need a product pass and a networking correctness pass.

## Current Problems Observed

### Host Session Screen

- The `Publish To Browser` control appears in a strange horizontal position.
  It looks like it may have been intended to sit near the bottom action area,
  between `Back` and `Host`, or otherwise be grouped with host actions.
- `Local Game` is unlabeled, so it is not clear that it is the advertised room
  name.
- The port value, such as `35355`, is unlabeled, so it is not clear that it is
  the direct host port.
- `Local Game` is a bad default advertised name. If the host publishes the room
  to the browser, the game is no longer meaningfully "local".
- `Visibility` is unclear. It is confusing to publish a room to the browser
  while the room says `Private`.
- It is not clear what happens when the host edits settings/name and presses
  `Host` again. The expected behavior is that the same host application updates
  or replaces its existing hosted room, not leaks stale rooms.
- After hosting, there is no clear hosted-lobby status panel showing who has
  joined.
- The host has no clear indication of whether they are direct-hosting,
  publishing through `gubsy-roomd`, or hosting through a future backend such as
  Steam.

### Browser Publishing And Room Dashboard

- `gubsy-roomd` reports room updates, but the browser dashboard can still show
  "no public games are active".
- Need to verify whether rooms are being marked private, hidden, expired, or
  not joined to the active public list correctly.
- Users should be able to join an active public game from the browser view.
- Decide whether in-progress games remain joinable. If yes, they should stay
  visible while joinable. If no, the UI should show that they are in-progress
  and unavailable rather than silently disappearing.

### Top-Level Lobby Navigation

Current top-level organization is confusing:

- `Players`
- `Game Settings`
- `Host Game`
- `Browse Servers`

Desired top-level organization:

- `Players`
- `Game Settings`
- `Host Game`
- `Join Game`

`Join Game` should then contain:

- `Join By IP`
- `Browse Servers`

This separates hosting from joining and avoids making `Browse Servers` feel like
the only join path.

### Join By IP Screen

- Direct join fields are unlabeled or poorly labeled.
- `Join Direct` is awkwardly named.
- Desired screen:
  - One field for IP/host.
  - One field for port.
  - Bottom-right action button labeled `Join`.
  - Validation/error state if the IP/port combination cannot be reached.
  - Consider making the button disabled or red/error-styled when the input is
    malformed or the last connection attempt failed.

### Browse Servers Screen

- The browser servers view is too rough for normal use.
- It should present available public rooms as a server browser, not as a debug
  endpoint list.
- Users should be able to tab/select through available servers.
- Each server row/card should show enough information to choose a room:
  - Room name.
  - Host/player or client label if available.
  - Player count.
  - Max players.
  - Visibility/joinability.
  - Backend/source, such as `gubsy-roomd`.
  - Direct endpoint if exposed.
  - State: lobby, in progress, full, stale, private, or unavailable.

Status:

- The Gubsy `Browse Servers` screen now has a normal empty state instead of
  blank cards when no public games are listed.
- Room cards now show room name, host fallback, player count/max players,
  lobby/in-game state, `gubsy-roomd` backend/source, and realtime endpoint when
  one is exposed.
- Follow-up remains: decide whether in-progress/full rooms should be joinable,
  visible but disabled, or hidden.

## Desired Host Semantics

### Room Name

Default room names should be generated instead of `Local Game`.

Use a small deterministic/random word-list generator:

- Two adjectives plus one noun.
- Words should be common, readable, and silly enough to be memorable.
- Rough target: about 128 common words total is enough for now.
- Examples:
  - `Brave Wobbly Lantern`
  - `Tiny Bright Tunnel`
  - `Sleepy Golden Rope`

The host can still edit the name manually.

Status:

- Implemented in Gubsy. `gubsy_lobby_ensure_ready` now generates a default
  two-adjective-plus-noun room name instead of using `Local Game`.
- `lobby_online_smoke` verifies the generated name is not empty, is not
  `Local Game`, and is preserved when a public room is listed.

### Visibility

Define exact semantics:

- `Public`: published to the room browser and visible in public room lists.
- `Private`: publish/update metadata to the backend only if needed for direct
  joins or invites, but do not show in public listings.

If the selected action is `Publish To Browser`, default visibility should
probably be `Public`.

If the player chooses `Private`, the UI must explain through labels/structure
that the room is not browser-visible. Avoid the contradictory feeling of
"published to browser but private".

Status:

- The Gubsy `Host Public` menu command now forces `Public` before creating the
  room, matching `gubsy-roomd`'s public-list rule that only rooms with
  `privacy > 0` appear in `/rooms`.
- `lobby_online_smoke` now verifies a public hosted room is visible through the
  room list with `privacy > 0`.
- Follow-up remains: decide and implement exact behavior for private backend
  records/invites, since the current UI only exposes direct private hosting and
  public room publishing.

### Host Update Behavior

If the same running host application presses `Host` again:

- It should update or replace the existing host session.
- It should unpublish or update the previous room record in `gubsy-roomd`.
- It should not leave stale room records behind.
- The host token/update token should remain tied to the current room lifecycle.
- Rehosting should cleanly close/reopen the old socket or reuse it deliberately;
  whichever behavior we choose should be documented in code and UI behavior.

If a different host starts a different game:

- It should create a distinct room.
- The room browser should show both public joinable rooms if both are public and
  still alive.

Status:

- Gubsy now leaves any existing online session before starting a new direct or
  public host session, so pressing host again replaces the current host session
  instead of leaving the old room/transport alive.
- `lobby_online_smoke` verifies public rehosting leaves the previous room,
  restarts host transport, keeps the new room online, and removes the old room
  from the public list when the room code changes.
- Follow-up remains: expose this behavior more clearly in UI copy and add a
  focused multi-host smoke if we need stronger proof that separate host
  processes list as separate public rooms.

### Hosted-Lobby Status

After hosting, the host should see clear state such as:

- `Currently Direct Hosting`
- `Currently Public Hosting via gubsy-roomd`
- `Currently Hosting via Steam` when that backend exists

Put this in a stable location, likely top-right or another consistent status
area.

The status should include:

- Room name.
- Backend.
- Direct endpoint when applicable.
- Room code or browser identifier when applicable.
- Player count.
- Joinability state.

Status:

- The top-level Gubsy lobby now includes a stable session status line showing
  offline state, direct hosting, public hosting via `gubsy-roomd`, joined direct
  game, or joined public game.
- The status includes the room name, room code when present, realtime endpoint
  when present, and local-player count.
- Follow-up remains: add remote player/client counts and richer joinability
  state once remote member listing is surfaced in the player menu.

## Players Menu

Prefer one unified `Players` menu instead of separate `Local Players` and
`Remote Players` top-level entries.

The top-level `Players` row can summarize:

- `N local`
- `N remote`
- Supporting text such as: `Manage profiles, devices, binds, and connected
  players.`

Inside the `Players` screen:

- Show local players first.
- Show remote connected players below local players.
- Sort/group remote players by connected client.
- For local players, selecting a player opens the current local profile/device
  management flow.
- For remote players, selecting a player opens remote management actions.

Remote player details should show:

- Display name.
- Client identifier.
- Connection endpoint/backend if available.
- Join time or last-seen time if useful.
- Whether the player is ready/in lobby/in game.

Remote management actions should include at least:

- Kick.
- Ban/block, when we have a real persistence model for it.

If this unified menu becomes too crowded, split into tabs inside `Players`
rather than separate top-level entries:

- `Local`
- `Remote`
- `Clients`

Status:

- The top-level Gubsy `Players` row now summarizes local players plus remote
  room members when matchmaking member data is available.
- The `Players` screen now lists local players first and remote room members
  below them, with host/client labels and member identifiers as early remote
  client context.
- Follow-up remains: remote player rows are informational only for now; kick,
  ban/block, richer endpoint/backend detail, and local/remote/client tabs are
  still pending.

## Join And Leave Alerts

Add lightweight alerts/toasts for lobby membership changes:

- `<player> joined from client <client>`
- `<player> left`
- `<player> disconnected`
- `<client> connected`
- `<client> disconnected`

Alerts should be visible to the host and probably to all lobby members where
appropriate.

The alert system should avoid leaking confusing internal IDs when a nicer name
is available, but internal client IDs are acceptable as a fallback during early
development.

Status:

- Gubsy now refreshes the current room member snapshot after public room create,
  public room join, and successful online heartbeats.
- Heartbeat membership refresh compares old and new member IDs and emits
  lightweight joined/left alerts using display names when available, with
  member IDs as fallback.
- `lobby_online_smoke` verifies the host sees membership grow when a guest joins
  and shrink when the guest leaves, and verifies joined/left alerts are emitted.
- Follow-up remains: direct-IP sessions do not have room-service membership
  records yet, and alerts still use room-member/client-level identities rather
  than per-player names.

## Client Experience

### Joined Client Should Not Control Host Settings

If a player is joined to somebody else's lobby:

- Game settings should be read-only or hidden unless the host grants control.
- Host-only actions should not be available.
- The client should see who the host is and what backend/room they joined.
- The client should see player counts and lobby membership.

Status:

- Gubsy game settings already mark host-owned rows read-only when the runtime
  is joined as a non-host client.
- The top-level lobby now labels joined clients as joined/waiting and prevents
  them from opening the host-game flow while they are connected to another
  host.
- Follow-up remains: show fuller host/backend/player-count context in the
  top-level lobby once remote member/client listing is available.

### Start Game Button For Joined Clients

The `Start Game` button is currently available to joined clients. When pressed,
the client immediately goes to a `loading screen`.

Desired behavior:

- Joined clients should not start the game directly unless we intentionally add
  a ready/vote system.
- If the host has not started, the client should see `Waiting For Host To Start`
  or similar.
- If the client is waiting for host transition/load sync, the UI should say that
  explicitly, not generic `loading screen`.
- Host-only `Start Game` should be visually disabled or replaced by a ready
  state for clients.

Possible future behavior:

- Clients can toggle `Ready`.
- Host sees ready count.
- Host starts when ready, or forces start.

Status:

- Gubsy now blocks non-host joined clients from starting the game through both
  the lobby menu and `gubsy_start_lobby_game`.
- Joined clients see `Waiting For Host` in the top-level lobby action slot, and
  the public API returns `Waiting For Host To Start` without invoking the start
  callback.
- `public_api_smoke` covers the non-host joined-client start rejection.

## Client Movement Regression

Connected clients currently cannot move after a game starts; only the host can
move.

This is a regression from before the Gubsy integration and must be treated as a
blocking gameplay/network bug.

Status:

- Fixed initial joined-client movement by mapping Gubsy gameplay input to the
  assigned network `PlayerId` instead of the local player slot index.
- Fixed host-triggered multiplayer restart preserving the wrong local/remote
  player ownership on peers.
- Added a host-side input ownership guard so peers cannot submit input records
  for player IDs not assigned to their endpoint.
- Added focused smoke coverage for the restart-style fresh network stage reload
  preserving host/local and peer/local ownership.
- The broad `--check-input-lockstep-smoke` still has a separate frame-0 input
  flag mismatch and remains a follow-up determinism test issue.

Investigation areas:

- Verify Gubsy input forwarding for joined clients after the lobby transitions
  into gameplay.
- Verify the local client still owns and submits input for its assigned network
  player IDs.
- Verify player/device bindings survive the Gubsy lobby-to-game transition.
- Verify the joined client's input is sent over the Splonks network session
  after start.
- Verify host receives and applies remote input packets.
- Verify the Gubsy shell does not keep menu/input suppression enabled after
  entering gameplay.
- Verify `state.suppress_gameplay_input` and in-game menu state are reset
  correctly for joined clients.
- Add or update smoke coverage for joined-client gameplay movement, not just
  lobby host/join.

Expected fix outcome:

- Host can move their local player.
- Joined client can move their local assigned player.
- Host sees joined client movement.
- Joined client sees host movement.
- After a host-triggered restart, host and clients keep the same local/remote
  player ownership.
- After a host-triggered restart, the host continues syncing canonical input to
  clients and clients only submit their own assigned player inputs.

## Room Browser Backend Checks

Investigate why `gubsy-roomd` can show update traffic while the browser
dashboard says no public games are active.

Checklist:

- Confirm the host sends `public` visibility when publishing to browser.
- Confirm `Private` rooms are intentionally hidden from `/rooms`.
- Confirm the dashboard lists the same room collection used by game clients.
- Confirm room TTL/heartbeat does not expire active hosts too aggressively.
- Confirm host update token enforcement is not rejecting updates silently.
- Confirm second host creates a separate room rather than overwriting the first
  due to shared identity/default room code.
- Confirm in-progress rooms remain listed if they are intended to be joinable.
- Improve room server logs around create/update/list filtering decisions.

## Proposed Implementation Order

1. Fix labels and layout on `Host Game`, `Join By IP`, and `Browse Servers`.
2. Replace `Local Game` default with generated room names.
3. Define and implement public/private room visibility semantics.
4. Add hosted-lobby status display.
5. Reorganize top-level lobby navigation to `Players`, `Game Settings`,
   `Host Game`, `Join Game`.
6. Add `Join Game` submenu with `Join By IP` and `Browse Servers`.
7. Build a usable server browser row/card view.
8. Unify player management under `Players`, showing local and remote counts.
9. Add remote player/client listing and basic kick action.
10. Add join/leave alerts.
11. Fix client-side start-game behavior and waiting-for-host messaging.
12. Fix the joined-client movement and host-triggered restart ownership
    regressions.
13. Add smoke coverage for browser-published host/join and joined-client
    movement after game start.

## Open Questions

- Should in-progress public games remain joinable, visible but disabled, or
  hidden?
- Should `Private` rooms exist in `gubsy-roomd` at all, or should private mean
  direct-IP only with no backend record?
- Do we want invite codes separate from public browser visibility?
- What persistent identity should a client have for kick/ban?
- Should remote player management be available only to the host, or also to
  local co-op players on the host machine?
- Should clients have a `Ready` button, or should only the host control start?
