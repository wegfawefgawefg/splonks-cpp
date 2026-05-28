# Lobby Human Playtest Checklist

Purpose: capture the remaining real-window evidence for `plan_3_lobbywork.md`.
Automated smokes cover the backend, widget layout, direct transport, and lockstep
paths. This checklist verifies the actual player-facing lobby flow with two
Splonks instances.

## Setup

Run:

```sh
scripts/run_lobby_human_playtest.sh --fill-verdict
```

The launcher builds Splonks if needed, starts a local `gubsy-roomd`, sets
`GUB_ROOM_SERVER_URL` for both Splonks instances, and opens two game windows.
Use one as host and one as client.

It also initializes `logs/lobby_human_playtest_verdict.json` from
`docs/lobby_human_playtest_verdict_template.json` when `--init-verdict` or
`--fill-verdict` is set. With `--fill-verdict`, it prompts for the verdict and
runs the audit after both game windows close. You can also run those steps
manually:

```sh
scripts/fill_lobby_human_playtest_verdict.py
scripts/summarize_lobby_human_playtest.py
```

If testing across two machines, run `gubsy-roomd` on the host machine and start
both games with the same `GUB_ROOM_SERVER_URL`, for example:

```sh
GUB_ROOM_SERVER_URL=http://192.168.11.7:8788 scripts/run.sh
```

## Public Browser Flow

Host window:

- Open `Host Game`.
- Confirm `Room Name` and `Host Port` are labeled and the values appear on the
  value line, not as the field title.
- Select `Host Public`.
- Confirm the main lobby shows public hosting status and `Stop Hosting` in the
  bottom action area.
- Confirm `Players` remains the row title.

Client window:

- Open `Join Game`, then `Browse Servers`.
- Confirm the top input is `Search Servers`.
- Confirm `Refresh` is in the bottom action row near `Back`.
- Search by part of the room name and confirm the visible list filters.
- Select the host room.
- Confirm the joined lobby hides normal `Host Game` and `Join Game`.
- Confirm `Leave Session` is the bottom exit action.
- Confirm the primary action is `Waiting For Host` before the host starts.

Host window:

- Start the hosted game.

Client window:

- Confirm the action stays `Waiting For Host` until local catchup is ready.
- Confirm the action becomes `Play`.
- Select `Play`.
- Confirm the client leaves the lobby and enters gameplay or the loading
  transition.
- Confirm input moves the joined client's player once gameplay is active.

## Direct Join Flow

Client window, with no direct host running:

- Open `Join Game`, then `Join By IP`.
- Confirm `IP / Host` and `Port` are labeled.
- Try an unused endpoint.
- Confirm failure stays on `Join By IP`.
- Confirm the action reports `No Server Found` or equivalent clear failure
  copy.
- Confirm the lobby does not claim `Joined Direct Game`.

Host window:

- Open `Host Game`.
- Select `Host Direct`.

Client window:

- Enter the direct host endpoint.
- Join successfully.
- Confirm joined lobby state is truthful and host/join actions are hidden while
  in the session.

## Alerts

Verify alert toasts render at the top of the screen and stack without covering
each other for:

- Join started/succeeded/failed.
- Player joined.
- Player left or disconnected.
- Host started game.
- Entering hosted game.
- Leaving session or stopping hosting.

Also verify alerts render over active gameplay with the menu closed.

## Pass Criteria

- Public browser flow works end to end.
- Direct join failure and success are truthful.
- Joined-session lobby state only exposes valid actions.
- Client can enter gameplay after host start.
- Alerts are visible and understandable in lobby and in-game.
- Any failed, confusing, or clipped UI state is recorded back into
  `docs/plan_3_lobbywork.md` before marking the goal complete.
- `scripts/summarize_lobby_human_playtest.py` reports `ok` for the filled
  verdict JSON.
