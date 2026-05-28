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

## Client Experience

### Joined Client Should Not Control Host Settings

If a player is joined to somebody else's lobby:

- Game settings should be read-only or hidden unless the host grants control.
- Host-only actions should not be available.
- The client should see who the host is and what backend/room they joined.
- The client should see player counts and lobby membership.

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

## Client Movement Regression

Connected clients currently cannot move after a game starts; only the host can
move.

This is a regression from before the Gubsy integration and must be treated as a
blocking gameplay/network bug.

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
12. Fix the joined-client movement regression.
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

