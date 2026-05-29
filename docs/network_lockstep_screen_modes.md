# Network Lockstep and Screen Modes

## Problem

Network lockstep must not be coupled to individual UI or game screen modes.

The stage transition text cleanup exposed this bug: while the host was on a
stage transition screen, it could stop maintaining old-stage lockstep packets
before peers received the final input frames needed to enter the same
transition. The host then loaded the next stage and bumped `stage_instance_id`,
while the peer remained on the previous stage. From that point, both sides
ignored each other's packets because they belonged to different stage
instances.

The bad architectural shape is:

- `Playing` advances lockstep and pumps packets.
- `StageTransition` sometimes pumps packets.
- Other future screens would each need their own networking patch.

That does not scale. Shops, score screens, post-level screens, pause overlays,
lobbies, and loading/sync screens should not each define bespoke network
transport behavior.

## Rule

Active network sessions maintain transport every frame, independent of the
visible screen.

Simulation stepping is separate:

- Transport maintenance pumps incoming packets, sends pending control packets,
  resends buffered input history, flushes fuzzer queues, and updates transport
  stats.
- Lockstep simulation stepping consumes a ready input frame and advances game
  state.

A screen may pause simulation. A screen must not accidentally stop transport.

## Barriers

This rule should remove bespoke networking behavior from screen modes. It does
not remove protocol barriers for deterministic simulation changes.

Screens should not own barriers just because they are visible. The protocol
should own barriers when the deterministic contract changes.

Screen states that should not need bespoke barriers:

- Showing a stage splash.
- Showing a score or post-level screen.
- Showing a shop UI.
- Showing a pause or menu overlay.
- Waiting for local player confirmation.
- Changing the text from "Loading" to "Synchronizing" or "Press jump to continue".

Protocol events that may still need barriers or explicit coordination:

- Adding players to the active lockstep player set.
- Removing players from the active lockstep player set.
- Loading a new stage, seed, or `stage_instance_id`.
- Catching up a late joiner from a host snapshot.
- Applying deterministic settings changes.
- Restarting a network run.

The join barrier is an example of a protocol barrier: it coordinates topology
and snapshot catchup. A leave flow may not need a full join-style barrier, but
removing a client or that client's players still needs host-authoritative
topology removal so every peer agrees on the required input set.

## Target Model

The main frame flow should eventually look like this:

```cpp
if (network::IsInputLockstepActive(state)) {
    network::MaintainInputLockstepTransport(state, graphics);
}

if (ModeAdvancesLockstepSimulation(state.mode)) {
    if (!network::PrepareInputLockstepFrame(state, graphics)) {
        return;
    }
}

StepCurrentMode(state, audio, graphics);
```

`PrepareInputLockstepFrame()` should be the path that advances one lockstep
simulation frame when possible.

`MaintainInputLockstepTransport()` should be the path that keeps network
transport alive while the current screen is not advancing simulation.

## Screen Text

Screen copy should be driven by state, but it should not change network
semantics.

Examples:

- Local transition delay: show the stage title, then the continue prompt.
- Network stage transition: show sync/progress text when peers are catching up.
- Join-in-progress: show snapshot or player synchronization progress.
- Post-level score/shop screens: can wait for player confirmation while network
  transport keeps pumping.

These are rendering decisions. They should not decide whether packets are sent.

## Current State

`MaintainInputLockstepTransport()` exists and is currently called from
`StageTransition` when a network lockstep session is active but not catchup
blocking. That fixed the concrete stage-door freeze where the host entered the
next stage before the peer received final old-stage input frames.

This is a step toward the target model, but not the final shape. The follow-up
cleanup is to move transport maintenance higher in the frame loop so every
screen gets the rule by default.

## Follow-Up Work

- Move lockstep transport maintenance to a single high-level per-frame site.
- Keep `PrepareInputLockstepFrame()` responsible only for advancing simulation.
- Add a clear helper for whether a mode advances lockstep simulation.
- Recheck `StageTransition`, `GameOver`, future score screens, shops, pause
  overlays, and lobby overlays against this rule.
- Keep screen copy/state labels in render code or UI state helpers, not in
  network control flow.
