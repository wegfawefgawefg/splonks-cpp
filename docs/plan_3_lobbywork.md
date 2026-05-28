# Plan 3 Lobby Work

## Goal

Make the Gubsy/Splonks lobby screens feel understandable and make online join
state truthful. The current UI can visually imply a successful direct join even
when no host is actually available, and several controls still use text-entry
layout in a confusing way.

## Problems Observed

### Join By IP Text Fields

The `Join By IP` screen shows two text boxes with values like `127.0.0.1` and
`35355`, but the boxes are unlabeled in the rendered UI. The only way to infer
their meaning is from context.

Text fields should follow the same hierarchy as option rows like `Max Players`:

- Title/label at the top of the box.
- Grey descriptive/help text in the middle when useful.
- Editable value on the bottom line.

The current behavior appears to edit the title/label line itself. That is
confusing, especially because unlabeled text boxes make it unclear whether the
user is editing host, port, room name, or some other field.

Required changes:

- Label the direct host field as `IP / Host`.
- Label the port field as `Port`.
- Render text input values on a value line, not in place of the field label.
- Preserve useful helper copy without pushing the editable value into the title
  position.

### Direct Join Feedback And Validation

Pressing `Join` currently returns through menu screens and can leave the lobby
claiming something like `Joined Direct Game` / `Joined direct 127.0.0.1:35355`
even when no actual host is running at that address. This is a state-machine
bug, not just a copy issue.

Expected behavior:

- A direct join must not mark the runtime online unless a real game transport
  connection succeeds.
- If no server is reachable at the typed IP/port, the player should stay on the
  `Join By IP` screen and see clear failure copy such as
  `No server found at 127.0.0.1:35355`.
- The failure should not pop back to `Join Game` or the main lobby.
- The lobby must not show `Joined Direct Game` unless the direct transport is
  connected.
- The `Join` action should be disabled, muted/red, or otherwise clearly not
  joinable when the address is malformed or known unreachable.

Preferred live validation behavior:

- As the user edits IP/host or port, probe/check the address when practical.
- Show `Server Found` in the join action description when the endpoint is
  reachable.
- Show `No Server Found` or `No server found at <ip>:<port>` when unreachable.
- Only make the `Join` button green/actionable when a server is actually found.
- Keep the check lightweight and avoid blocking typing or menu navigation.

### Joined Lobby State

After a successful client join, the lobby should expose only actions that make
sense for a joined client.

Expected joined-client lobby behavior:

- `Players` remains available.
- `Game Settings` may remain visible but should be read-only/host-owned unless
  we add explicit host delegation.
- `Host Game` should not be available while joined to another host.
- `Join Game` should not be available while joined to another host unless we
  deliberately support "leave current session and join another" from that path.
- `Leave Session` should be the obvious way out of the joined state.
- More generally, `Host Game` and `Join Game` should only appear when the
  runtime is not already in an online/direct/public session. If we are in a
  session, the user should leave that session before seeing normal host/join
  entry points again.

Current issue:

- The joined lobby can still show `Host Game` and `Join Game` in the main list,
  with `Host Game` disabled via copy. This feels wrong for the normal joined
  client state. Prefer hiding host/join actions, or moving any rejoin/switch
  behavior behind an explicit leave-first flow.

### Joined Client Play Transition

When a client joins through the room browser, the lobby can show
`Waiting For Host` / `Only the host can start`. If the host later starts the
game, the client can receive host state but remain stuck on the lobby view.

This is wrong for Splonks' current multiplayer model. Joined clients should be
able to enter play once the host is actually in a playable state. That can mean
the host is at the initial level transition/load state or already in the middle
of a level, because this game supports join-in-progress behavior.

Expected behavior:

- While the host is still in lobby, joined clients see `Waiting For Host`.
- Once the host is in play or ready for a joined client to load in, the disabled
  waiting action should become an actionable `Play` or equivalent.
- The `Play` action should be green/actionable and should transition the joined
  client into gameplay using the host-provided state.
- The client should not stay stuck on the lobby after host state has arrived and
  the session is playable.
- If the host has not reached a playable state or state sync is incomplete, the
  UI should say that clearly rather than pretending the only possible action is
  host-owned start.

Open implementation question:

- Decide whether the client auto-enters play when the host sends playable state,
  or whether the button changes to `Play` and the client confirms. Current
  preference is a clear green `Play` button because it makes the state machine
  visible while we are still debugging.

### Local Start Versus Online Start

The main lobby start action should be explicit about whether it is starting a
local-only game or starting/entering an online session.

Expected behavior:

- If the runtime is not connected to a remote session through `Host Game` or
  `Join Game`, the primary start action should say `Start Local Game`.
- `Start Local Game` should launch local play without implying public/direct
  hosting or a joined room.
- If the runtime is hosting, the host action should say `Start Game` or
  `Start Hosted Game` and should notify connected clients.
- If the runtime is joined as a client and the host has not started, the action
  should be a disabled/waiting state such as `Waiting For Host`.
- If the runtime is joined as a client and the host state is playable, the
  action should become `Play` or equivalent and enter the synced game.

This distinction matters because the lobby is now used for local play, direct
hosting, public hosting through `gubsy-roomd`, and browser/direct joining. The
button text should describe the actual mode rather than always using generic
`Start Game` copy.

### Browse Servers Search And Refresh

The `Browse Servers` view should reserve the top input area for filtering
visible servers by room name. A room browser can plausibly have dozens or
hundreds of rooms, so name search should be a first-class control.

Required behavior:

- Add a text search box at the top of the server browser.
- Search/filter by server or room name as the user types.
- Keep filtering local to the currently fetched server list unless we later add
  backend-side search.
- Keep the visible list stable and navigable after filtering.
- Empty search text should show all currently visible public rooms.

Refresh placement:

- Move `Refresh` out of the top search area.
- Put `Refresh` in the bottom action row, near the center and next to `Back`.
- Keep bottom actions consistent with the other lobby screens.
- Continue showing refresh/load/failure state clearly, preferably with alerts
  for errors and a small status line for current results.

### Leave Session Placement

`Leave Session` currently appears in a card row over the top list area, making
it look like it is layered into the wrong position. It should be a bottom action
like other primary session commands.

Required changes:

- Put `Leave Session` in the bottom action area for joined clients.
- Put `Stop Hosting` in the bottom action area for hosts.
- Avoid placing leave/stop commands over the `Players` row or other list cards.

### Host Session Text Fields

The `Host Session` screen has the same text-field hierarchy problem as
`Join By IP`.

Observed issues:

- The room name appears as an unlabeled editable box.
- The port appears as an unlabeled editable box.
- The user appears to edit the title line, not a value line.

Required changes:

- Label room name as `Room Name`.
- Label direct hosting port as `Host Port`.
- Render editable text values on a value line, matching the `Max Players`
  option-row pattern.
- Ensure the generated room name remains visible as the value, not as the field
  label.

### Host Public Button Placement

`Host Public` still appears too high/inside the form area. It should be grouped
with the bottom host actions.

Required changes:

- Move `Host Public` down into the bottom action row.
- Keep `Back`, `Host Public`, and `Host Direct` visually grouped as bottom
  commands.
- Make sure text does not clip in the bottom actions at 1280x720.

### Alert / Toast System

The game needs a cheap general-purpose alert system: short messages that slide
or appear from the top of the screen, stack vertically, last for a configured
duration, and then disappear automatically.

This should be useful for lobby/network events, gameplay events, errors, and
debug notifications. Right now the host has no strong way to notice that a
client joined, left, disconnected, got kicked, started loading, or entered the
game except by watching player rows change.

Required behavior:

- Store alerts in a small runtime list.
- Each alert stores message text, creation time, duration, and a color/severity.
- Step/update alerts every frame.
- Remove alerts once `now - created_at >= duration`.
- Render active alerts stacked from the top of the screen.
- Newer alerts should stack cleanly without covering each other.
- Alerts should be cheap and safe to emit from many systems.

Suggested severities/colors:

- Info: neutral/white/blue for normal state changes.
- Success: green for successful joins, server found, game entered.
- Warning: yellow/orange for recoverable issues.
- Error: red for failed joins, disconnects, invalid state, server not found.
- Debug: muted grey/purple for development-only diagnostics if enabled.

Initial alert use cases:

- Joining lobby started.
- Lobby join succeeded.
- Lobby join failed / no server found.
- Joining game / loading host state.
- Entered game.
- Player joined.
- Player left.
- Player disconnected.
- Host started game.
- Host stopped hosting.
- Client left session.
- Client kicked / removed.
- Debug networking messages while developing lobby flow.

Implementation notes:

- Gubsy already has some alert/message concepts for menu smokes; use or replace
  them with a real rendered stacked-alert surface.
- Alerts should be available from both Gubsy shell/menu code and Splonks
  gameplay/network code.
- Keep lifetime and rendering deterministic enough for smoke tests.
- Add a small cap, such as 6-8 visible alerts, to prevent spam from covering
  the whole screen.
- If many alerts arrive at once, expire oldest first or compact into a debug
  count later. Do the simple list first.

## Implementation Notes

- This is a Gubsy UI/state pass surfaced through Splonks. Most fixes likely
  belong in Gubsy menu widgets, Gubsy lobby state, and Splonks direct transport
  callbacks.
- Do not trust menu-copy-only success. A direct join result must reflect the
  actual transport result.
- The direct endpoint probe should be separated from the final join command if
  possible, so typing/validation does not accidentally mutate lobby state.
- Add smoke coverage for the false-positive direct join case: no server running
  should not set `online`, should not pop to lobby, and should show failure
  status.
- Add rendered/widget smoke coverage for text-input row hierarchy and bottom
  action placement.
- Add smoke coverage for a browser-joined client receiving host playable state
  and being able to enter gameplay instead of remaining stuck in the lobby.
- Add widget smoke coverage proving `Host Game` and `Join Game` are absent or
  unavailable as normal main-list actions while already in an online session.
- Add smoke/render coverage proving alerts stack, expire, and render with
  severity color.
- Add rendered/widget smoke coverage for `Start Local Game` copy when offline
  and online/joined start-state copy when hosted or joined.
- Add rendered/widget smoke coverage for server-browser name search and bottom
  refresh placement.

## Done When

- Host and Join text inputs have labels, helper text, and value-line rendering.
- Direct join cannot report success without a real reachable host.
- Failed direct join stays on `Join By IP` with clear error copy.
- The join action communicates `Server Found` / `No Server Found` or equivalent
  reachability state.
- Joined clients do not see normal host/join actions as available main lobby
  actions.
- Browser-joined clients can enter play after the host starts or otherwise
  reaches a playable join-in-progress state.
- Offline lobby start action says `Start Local Game`; hosted/joined states use
  mode-accurate start/play/waiting copy.
- Timed stacked alerts render from the top of the screen and are used for
  join/leave/disconnect/joining-game/error events.
- Browse Servers has a top room-name search box, and `Refresh` is a bottom
  action near `Back`.
- `Leave Session` / `Stop Hosting` are bottom actions and do not overlap list
  rows.
- `Host Public` is grouped with bottom host actions.
- Targeted smoke/render checks cover the above.

## Implementation Status

Current code status:

- Done in Gubsy: `Join By IP` text fields are labeled `IP / Host` and `Port`,
  and labeled text inputs render their editable value below helper copy instead
  of replacing the title line.
- Done in Gubsy: `Host Session` text fields are labeled `Room Name` and
  `Host Port`, with the generated room name shown as the editable value.
- Done in Gubsy/Splonks: direct join does not mark the lobby online until the
  Splonks transport accepts the join. Failed direct joins stay on `Join By IP`
  and report `No server found at <ip>:<port>`.
- Done in Gubsy: after a failed endpoint-specific direct join, the action is
  disabled and shown as `No Server Found` until the user edits the endpoint.
- Done in Gubsy: joined sessions hide normal `Host Game` and `Join Game`
  entries and expose `Leave Session` as the bottom exit action.
- Done in Gubsy/Splonks: joined clients show `Waiting For Host` until host
  state is playable, then the action becomes `Play` and enters the synced game.
- Done in Gubsy: offline start says `Start Local Game`; hosted and joined
  states use `Start Game`, `Waiting For Host`, or `Play` as appropriate.
- Done in Gubsy/Splonks: alerts are runtime toasts with severity colors, finite
  duration, and a small cap. Splonks updates and renders them even while menus
  are closed, so gameplay join/leave/error alerts can appear over the game.
- Done in Gubsy: server browser has a top `Search Servers` text box that
  filters fetched rooms by name, and `Refresh` is a bottom action near `Back`.
- Done in Gubsy: `Leave Session` and `Stop Hosting` are bottom actions in
  lobby and host setup flows.
- Done in Gubsy: `Host Public` is grouped in the bottom action row with
  `Back` and `Host Direct` / `Stop Hosting`.
- Done in Splonks: public hosts continuously sync lobby versus in-game phase
  back to Gubsy so browser-joined clients can see when `Play` is available.

Intentional behavior:

- Direct join reachability is still confirmed by the actual Splonks join
  attempt and subsequent transport acceptance, not by a separate background
  UDP probe while typing. The button copy says it checks the endpoint, then
  the runtime records the truthful success/failure state.

Still needs manual verification:

- Run `gubsy-roomd`, host a public Splonks lobby, join from a second Splonks
  instance through `Browse Servers`, start from the host, and verify the client
  sees `Play` and enters gameplay.
- Repeat direct host/join with no server running first, then with a real host,
  and verify the failed state, joined state, and alerts are understandable.
- Verify in-game alerts are visible with menus closed for join, leave, kick,
  and host-start events.

Latest local validation:

- Passed: `gubsy ./scripts/room_smoke.sh`.
- Passed: `gubsy ./scripts/lobby_online_smoke.sh`.
- Passed after render-smoke strengthening: `gubsy GUBSY_RENDER_SMOKE=1 ./scripts/lobby_online_smoke.sh`.
  This checks rendered/widget placement for Host Public below the host form,
  browser Search Servers above room cards, bottom Refresh below room cards, and
  own hosted public rooms as unavailable `YOUR ROOM` rows.
- Passed after smoke update: `splonks-cpp ./scripts/build.sh`.
- Passed after smoke update: `splonks-cpp ctest --test-dir build --output-on-failure -R "gubsy_shell_smoke|gubsy_import_smoke|gubsy_binds_smoke"`.
- Passed after validation launcher update: `splonks-cpp ./scripts/validate_lockstep_live.py --launch-pair --profile same-house --ready-timeout 60 --report-json logs/lobbywork_lockstep_validate_report.json`.
  The launcher now starts the host into gameplay before launching the peer,
  waits for lockstep catchup, starts the peer, then runs the same-house input
  sequence with no hash mismatches or fatal desync.
- Passed after real-roomd smoke addition: `splonks-cpp ./scripts/validate_gubsy_roomd_live.sh`.
  This starts a local `gubsy-roomd`, hosts a public room through Splonks'
  Gubsy shell callbacks, verifies the room is listed by the real HTTP backend,
  joins from a second Splonks shell, and confirms the actual direct transport
  join path.
