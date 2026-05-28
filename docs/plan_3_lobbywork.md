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

Current issue:

- The joined lobby can still show `Host Game` and `Join Game` in the main list,
  with `Host Game` disabled via copy. This feels wrong for the normal joined
  client state. Prefer hiding host/join actions, or moving any rejoin/switch
  behavior behind an explicit leave-first flow.

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

## Done When

- Host and Join text inputs have labels, helper text, and value-line rendering.
- Direct join cannot report success without a real reachable host.
- Failed direct join stays on `Join By IP` with clear error copy.
- The join action communicates `Server Found` / `No Server Found` or equivalent
  reachability state.
- Joined clients do not see normal host/join actions as available main lobby
  actions.
- `Leave Session` / `Stop Hosting` are bottom actions and do not overlap list
  rows.
- `Host Public` is grouped with bottom host actions.
- Targeted smoke/render checks cover the above.
