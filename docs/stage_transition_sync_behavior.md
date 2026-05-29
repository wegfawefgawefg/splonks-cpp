# Stage Transition And Sync Screens

This note records the intended behavior for level intro, stage transition, and multiplayer join/catchup screens.

## Local Or Offline Play

- The between-level screen is a level intro/ready gate, not a loading screen.
- It should show the stage name and a literal prompt such as `Press [jump] to continue`.
- The stage is generated when the local player accepts the prompt.
- Do not show a fake loading bar for local/offline play unless stage generation becomes asynchronous and measurable.

## Joining A Host Already In Play

- The joining client should see the real join/catchup progress UI.
- Progress should use the existing join-barrier byte/chunk counters.
- When catchup completes, the client should enter play automatically.
- The client should not see a second `Press [jump] to continue` gate after joining an active game.

## Joining A Host On A Stage Transition Screen

- The joining client may see the real join/catchup progress UI while receiving host state.
- After sync, the client should match the host's transition state.
- If the host is waiting on the level intro prompt, the client should show a waiting-for-host state and should not advance independently.
- When the host advances, clients follow into play through the normal multiplayer transition path.

## Rendering Rule

- Local/offline transitions use stage title plus prompt text.
- Multiplayer catchup uses the real join-barrier overlay whenever it is active, including while the client is in `Mode::StageTransition`.
- Barrier mechanics should remain owned by networking code; presentation should only expose the current sync status.
