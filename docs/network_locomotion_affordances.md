# Network Locomotion Affordances

Purpose: define where the owning player is allowed to be more authoritative than the coordinator's exact delayed simulation. This is not durable world authority. It is bounded local locomotion authority for responsiveness.

The core problem: edge-sensitive movement states such as hanging, climbing, coyote jumps, and swimming can be correct on the owning client but missed by the coordinator because the coordinator sees input and position a few frames late. If the coordinator blindly rejects those transitions, the player snaps out of valid local movement and the game feels broken.

## Ownership Boundary

- The coordinator owns durable/shared outcomes: tile changes, entity spawns, damage, pickup/economy, shop aggro, exits, stage progression, and canonical snapshots.
- A free player is locally body-authoritative: the owning client may claim responsive body state for its own player.
- The coordinator validates claimed locomotion/body state with generous plausibility checks, then broadcasts the accepted body state.
- Peers render accepted coordinator state for remote players and do not independently decide remote hang/climb/jump states.
- Invalid claims are corrected by explicit coordinator repair. A delayed ordinary coordinator position echo is not by itself a repair command for the owning peer, because high-latency echoes otherwise fight local prediction and create host-position/local-position warping.
- Valid claims should be accepted even if exact coordinator replay missed the same ledge or coyote window.

This follows the same broad multiplayer goal as our Terraria/tModLoader-style networking: gameplay facts converge through coordinator-owned lanes, but local movement should not wait for a round trip or exact delayed replay.

## Authority Modes

Use these modes conceptually. They do not need to be a copied enum on every entity unless the code needs one later.

- `FreeLocalOwned`: ordinary walking, running, jumping, falling, hanging, climbing, and swimming. The owning client sends body claims; the coordinator validates and accepts/rejects.
- `CoordinatorExternal`: carried, held, attached, thrown by another entity, stunned, dead, crushed, teleported, stage-transition locked, or under scripted motion. The coordinator owns body state; owner input is still accepted as requests, but not as position authority.
- `CoordinatorInterrupt`: a short external-control window after damage, explosion impulse, projectile contact, forced drop, or throw. The coordinator owns initial body impulse/state; the owner regains free body authority once the interrupt resolves.

Rule: `FreeLocalOwned` is the only mode that may apply ordinary player body claims. A held/stunned/dead/thrown player must not be able to overwrite coordinator body state through the high-frequency player snapshot lane.

## Common Plausibility Checks

Apply these before accepting any player-owned locomotion claim:

- [ ] The claim is for a body owned by the sending player/local slot.
- [ ] Stage instance and player id match current coordinator state.
- [ ] The claim sequence/frame is newer than the last accepted locomotion claim.
- [ ] Claimed position is within a bounded distance from the coordinator's last accepted/predicted body position.
- [ ] Claimed velocity is within player movement limits plus allowed impulse margin.
- [ ] Claimed condition is compatible with current body state unless it is a known interrupt or recovery path.
- [ ] The final claimed AABB does not overlap solid tiles.
- [x] Reject if dead, hard-stunned, deactivated, or coordinator-carried unless the transition explicitly allows it.
- [x] Reject if the coordinator has the player held, attached, thrown, stunned, dead, or under stage transition.

The distance tolerance should be latency-aware, not exact-pixel. Initial target: roughly `1-2 tiles` for edge-sensitive attach claims, tighter for ordinary free movement.

## Ordinary Free Body Claim

Claim payload should include position, velocity, acceleration, grounded flag, movement flags, facing, coyote/fall timers, hang/climb timers, and sequence/frame.

Plausibility:

- [ ] The coordinator currently sees the player as `FreeLocalOwned`.
- [ ] Claimed AABB does not overlap solid coordinator tiles.
- [ ] Claimed position is near the current coordinator body within latency tolerance.
- [ ] Claimed velocity is within generous player movement/impulse bounds.
- [ ] If claimed grounded, the coordinator sees valid floor support or stage-bottom support at the claimed position.
- [ ] If claimed not grounded, the claim may still be accepted as ordinary falling/jumping as long as it is not teleporting or passing through solid tiles.

Reason: this is the Terraria-like fix for one-block ledges and micro-tap jumps. The coordinator should not re-decide whether a remote player barely made the platform from delayed inputs; it should accept a plausible final body state.

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

## Contact Damage And Local Hit Claims

Contact outcomes need the same latency policy as locomotion. If a peer must wait
150ms before seeing its own bat/knife/rock hit or its own body take spike/snake
damage, the game feels broken. If the peer directly mutates durable world state,
the run forks. The boundary is therefore: peers may predict and claim plausible
contact outcomes, while the coordinator still commits the durable result.

Authority split:

- A peer may claim damage against its own local player body. This covers spikes,
  fall, arrows, explosions, enemy contact, and other cases where lying mostly
  hurts the claimant.
- A peer may claim hits caused by a locally controlled interaction source: the
  local player, a held item, a thrown item still attributable to that player, a
  fired projectile, or a tool artifact.
- A peer should not freely claim contested player-vs-player harm. The
  coordinator should validate those with both players' latest accepted body
  snapshots.
- The coordinator owns durable outcomes: health, death, loot spawns, tile
  destruction, shop anger, money, ownership, and stage progression.
- The peer may immediately predict local presentation/body feel: damage flashes,
  stun pose, knockback, hit particles, and similar temporary visual response.
  Coordinator messages later repair or confirm the durable state.

Plausibility checks for coordinator-accepted contact claims:

- [ ] Source entity is owned by, held by, or causally attributed to the sending
  player.
- [ ] Target exists, is active, and is not already immune/dead in a way that
  makes the claim impossible.
- [ ] Source and target AABBs overlap, swept-overlapped recently, or are within a
  latency-aware tolerance of the claimed attack/projectile shape.
- [ ] Weapon/tool/use state is compatible with the claim: attack frame active,
  projectile contact timer active, thrown source still dangerous, or contact
  damage alignment valid.
- [ ] Damage type is compatible with target vulnerability.
- [ ] Interaction cooldowns make repeated claims legal.
- [ ] For local-player self-damage claims, accept generously unless the target is
  impossible to reach from its latest accepted body state.
- [ ] For player-vs-player claims, use stricter validation against both players'
  latest accepted body snapshots.

Implementation rule: do not add item-specific networking for each weapon. Use
the existing typed `DamageEntityAction` / `HitEntityAction` request lane, add
shared validation and prediction policy there, and let ordinary entity patches
carry the durable result back to every peer.

## Implementation Checklist

- [x] Extend the existing player snapshot/input lane to carry bounded hang/climb/jump claims.
- [ ] Track last accepted locomotion claim sequence per player.
- [ ] Add coordinator validators for ordinary free body, hang, glove/corner hang, climb attach, jump/coyote jump, swim impulse, and carried jump-out/release.
- [x] Apply valid hang/climb/jump claims to coordinator body state before fall-damage/contact resolution.
- [x] Apply valid ordinary free body claims to coordinator body state before fall-damage/contact resolution.
- [x] Broadcast accepted hang/climb/jump state through existing player body snapshots.
- [x] Do not hard-snap the owning peer to ordinary delayed coordinator free-movement snapshots.
- [ ] Reject invalid claims with explicit body repair, not ordinary delayed position echo and not special-case item logic.
- [ ] Add debug overlay for local predicted state versus coordinator accepted state.
- [ ] Add fake-transport tests for ordinary free movement over a one-block ledge, one-frame ledge grab, delayed climb attach, coyote jump under latency, carried-player body rejection, swim impulse under latency, thrown-player horizontal velocity preservation, and fall damage after accepted hang/climb/swim.
- [ ] Add peer-side predicted contact presentation/body response for local-player
  damage and local-source hits.
- [ ] Add coordinator-side plausibility validation for peer
  `DamageEntityAction` / `HitEntityAction` claims.
- [ ] Add packet/frame tests for peer local-source bat/knife/projectile hit,
  peer self-damage, rejected impossible hit, repeated-contact cooldown, and
  player-vs-player stricter validation.

## Attachment Item Use

Held/back item use must not route stale full holder patches back into the
owning peer as movement repairs. `UseHeldEntity` / `UseBackEntity` patches the
item state, not the holder/player body. The holder/player body stays on the
player snapshot/locomotion lane, where local prediction rules already apply.

Items that directly modify the holder's body opt into local attachment-use
prediction through their archetype. Examples:

- Jetpack applies local upward acceleration immediately.
- Cape applies local fall-speed limiting immediately.
- Web cannon applies local recoil immediately.

Durable consequences remain coordinator-owned. Peer-side canonical entity
spawns are still blocked by `world_ops::SpawnEntity`, and durable damage/tile/
economy changes still go through coordinator requests.

Current code status:

- Peer-owned `PlayerSnapshotsPacket` messages are still the transport for player inputs.
- If a peer snapshot claims ordinary free movement, `Hanging`, `Climbing`, or a fresh upward jump, the coordinator validates the claim through shared entity movement geometry before accepting the body state.
- Accepted hang/climb/jump claims reset coordinator fall timer for that body, preventing stale replay from creating bogus fall damage after a valid local attach or jump.
- Ordinary free body claims use the same shared validator instead of one-off interpolation exceptions.
- Carried, attached, thrown, stunned, dead, and destroyed bodies reject owner-local body claims; coordinator state wins for those external-control cases.
- This is intentionally not durable world authority; tile/entity/economy facts still go through coordinator-owned message lanes.

## Non-Goals

- Do not let clients author durable world mutations through this path.
- Do not accept arbitrary teleports as locomotion claims.
- Do not create item-specific networking for hang/climb/jump.
- Do not require exact coordinator frame replay for edge-sensitive movement.
