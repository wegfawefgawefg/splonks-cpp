# Network Locomotion Affordances

Purpose: define where the owning player is allowed to be more authoritative than the coordinator's exact delayed simulation. This is not durable world authority. It is bounded local locomotion authority for responsiveness.

The core problem: edge-sensitive movement states such as hanging, climbing, coyote jumps, and swimming can be correct on the owning client but missed by the coordinator because the coordinator sees input and position a few frames late. If the coordinator blindly rejects those transitions, the player snaps out of valid local movement and the game feels broken.

## Ownership Boundary

- The coordinator owns durable/shared outcomes: tile changes, entity spawns, damage, pickup/economy, shop aggro, exits, stage progression, and canonical snapshots.
- The owning client may claim responsive locomotion transitions for its own player body.
- The coordinator validates claimed locomotion transitions with generous plausibility checks, then broadcasts the accepted body state.
- Peers render accepted coordinator state for remote players and do not independently decide remote hang/climb/jump states.
- Invalid claims are corrected by coordinator repair. Valid claims should be accepted even if exact coordinator replay missed the same ledge or coyote window.

This follows the same broad multiplayer goal as our Terraria/tModLoader-style networking: gameplay facts converge through coordinator-owned lanes, but local movement should not wait for a round trip or exact delayed replay.

## Common Plausibility Checks

Apply these before accepting any player-owned locomotion claim:

- [ ] The claim is for a body owned by the sending player/local slot.
- [ ] Stage instance and player id match current coordinator state.
- [ ] The claim sequence/frame is newer than the last accepted locomotion claim.
- [ ] Claimed position is within a bounded distance from the coordinator's last accepted/predicted body position.
- [ ] Claimed velocity is within player movement limits plus allowed impulse margin.
- [ ] Claimed condition is compatible with current body state unless it is a known interrupt or recovery path.
- [ ] The final claimed AABB does not overlap solid tiles.
- [ ] Reject if dead, hard-stunned, deactivated, or coordinator-carried unless the transition explicitly allows it.

The distance tolerance should be latency-aware, not exact-pixel. Initial target: roughly `1-2 tiles` for edge-sensitive attach claims, tighter for ordinary free movement.

## Hang / Ledge Grab

Claim payload should include position, velocity, hang side, hang anchor tile or world anchor, facing, and sequence/frame.

Plausibility:

- [ ] Claimed ledge anchor is solid or one-way-valid.
- [ ] Claimed hang side matches the ledge side.
- [ ] Player was recently airborne/falling or leaving ground, not grounded deep inside terrain.
- [ ] Claimed hang AABB is near the anchor within latency tolerance.
- [ ] Final hang AABB is not inside solid.
- [ ] Reject while dead, hard-stunned, deactivated, or carried.

Reason: a client can correctly hit a one-frame ledge window that the coordinator misses. Exact replay is too strict for hanging.

## Glove Wall Hang / Forced Corner Hang

Plausibility:

- [ ] For glove hang, the player has the glove effect and the claimed wall side has a solid surface near the probe.
- [ ] For forced corner hang, the claimed geometry is a corner, not a flat wall.
- [ ] Claimed wall/corner is near the player within latency tolerance.
- [ ] Final AABB is not inside solid.
- [ ] Repeated attach/detach claims obey a short cooldown or monotonic sequence rule.

Rule: forced corner hang should not turn into side-grab via gloves, and glove side-grab should not bypass valid wall-side probes.

## Climb Attach

Claim payload should include climbable tile/anchor, intended attach direction, position, velocity, and sequence/frame.

Plausibility:

- [ ] A climbable tile exists near the claimed anchor.
- [ ] Normal climb probes, with tolerance, intersect climbable cells.
- [ ] For grounded down-grab, require a climbable below, or side-with-below rule.
- [ ] Clamp accepted snap distance; reject large lateral snaps.
- [ ] Final AABB is not inside solid.
- [ ] Reject while dead, hard-stunned, deactivated, or carried.

Reason: climbing attach is also edge-sensitive and uses probes that may differ by a few delayed frames.

## Jump / Coyote Jump

Claim payload should include jump sequence, pre-jump state, resulting velocity, and whether the jump came from ground, coyote, hang, climb, swim, or bounce.

Plausibility:

- [ ] Owning client had grounded/coyote/hang/climb/swim eligibility recently.
- [ ] Jump sequence is fresh and has not already been consumed.
- [ ] Resulting upward velocity is within configured jump impulse plus effect modifiers.
- [ ] Reject while dead, hard-stunned, deactivated, or carried unless this is a valid jump-out-of-carried action.
- [ ] For spring shoes/head bounce, require recent stomp/contact marker.

The coordinator should not accept repeated coyote jumps without a landing/reset.

## Swim Impulse / Fluid Movement

Plausibility:

- [ ] Client claim overlaps a fluid cell above the gameplay/render cutoff.
- [ ] Coordinator sees fluid at or near that area, with a small tile tolerance because fluids are dynamic.
- [ ] Impulse direction and magnitude fit water/liquid effect limits.
- [ ] Fall timer reset or slowdown is accepted only if recently in fluid.
- [ ] Reject while dead, hard-stunned, deactivated, or carried unless swimming while carried is intentionally supported.

Fluid effects should stay effect-driven; this doc only describes accepting the player body's responsive swim transition.

## Carried / Thrown Player

Plausibility:

- [ ] Holder relationship is coordinator-approved.
- [ ] Held player can request jump-out/drop through a legal action.
- [ ] Throw velocity comes from a coordinator-approved throw action.
- [ ] After release/throw, the thrown player regains local locomotion authority with inherited velocity preserved.
- [ ] Reject claims that erase throw velocity unless normal control/damping could plausibly have done so.

This prevents the coordinator from mowing over horizontal throw velocity while still allowing a thrown player to steer once free.

## Fall Damage

Fall damage is a shared gameplay outcome and should stay coordinator-owned.

Rules:

- [ ] Coordinator computes health/stun damage.
- [ ] Client may predict local fall damage presentation, but coordinator result wins.
- [ ] Coordinator fall timer must use accepted client locomotion states such as hang, climb, and fluid contact so stale remote replay does not create bogus fall damage.
- [ ] If accepted hang/climb/swim states reset or pause fall timer, the coordinator's body history must reflect that before damage calculation.

## Implementation Checklist

- [x] Extend the existing player snapshot/input lane to carry bounded hang/climb/jump claims.
- [ ] Track last accepted locomotion claim sequence per player.
- [ ] Add coordinator validators for hang, glove/corner hang, climb attach, jump/coyote jump, swim impulse, and carried jump-out/release.
- [x] Apply valid hang/climb/jump claims to coordinator body state before fall-damage/contact resolution.
- [x] Broadcast accepted hang/climb/jump state through existing player body snapshots.
- [ ] Reject invalid claims with normal body repair, not special-case item logic.
- [ ] Add debug overlay for local predicted state versus coordinator accepted state.
- [ ] Add fake-transport tests for one-frame ledge grab, delayed climb attach, coyote jump under latency, swim impulse under latency, thrown-player horizontal velocity preservation, and fall damage after accepted hang/climb/swim.

Current code status:

- Peer-owned `PlayerSnapshotsPacket` messages are still the transport for player inputs.
- If a peer snapshot claims `Hanging`, `Climbing`, or a fresh upward jump, the coordinator validates the claim through shared entity movement geometry before accepting the body state.
- Accepted hang/climb/jump claims reset coordinator fall timer for that body, preventing stale replay from creating bogus fall damage after a valid local attach or jump.
- This is intentionally not durable world authority; tile/entity/economy facts still go through coordinator-owned message lanes.

## Non-Goals

- Do not let clients author durable world mutations through this path.
- Do not accept arbitrary teleports as locomotion claims.
- Do not create item-specific networking for hang/climb/jump.
- Do not require exact coordinator frame replay for edge-sensitive movement.
