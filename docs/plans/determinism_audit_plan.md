# Splonks Determinism Audit Plan

## Purpose

Splonks multiplayer depends on deterministic lockstep. The FXP quest removes one
major source of cross-machine drift, but fixed-point math alone does not prove
the simulation is deterministic.

This plan tracks the broader audit for every other way gameplay can diverge
across platforms, compilers, frame rates, reconnect/resync paths, and local
settings.

## Relationship To FXP

The FXP quest owns the fixed-point library work and the migration away from raw
float bit hashing. This audit owns the wider pass after and alongside that work:
raw floats that remain authoritative, unstable iteration order, RNG boundaries,
serialization details, platform-sized values, time-dependent simulation, and
network topology barriers.

The expected end state is:

- Authoritative gameplay math is fixed-point, integer, discrete, or explicitly
  quantized.
- Render, camera, UI, audio, and cosmetic effects can remain float if they do
  not feed back into gameplay state.
- Gameplay fingerprints, snapshots, replays, and resync data use explicit,
  cross-platform representations.
- Join/leave/topology changes apply at deterministic barriers.

## Gameplay Float Audit

- [ ] Find all gameplay-affecting uses of `float` and `double`.
- [ ] Classify each use as simulation, render-only, UI-only, audio-only, debug,
      or data loading.
- [ ] Prioritize fields currently hashed in network fingerprints:
      position, velocity, acceleration, size, rotation, and gameplay timers.
- [ ] Replace or quantize simulation floats that affect branches, collision,
      spawning, damage, pickups, hazards, AI, or world mutation.
- [ ] Keep render/camera/UI/audio/effects float unless they feed back into
      gameplay state.

### Status 2026-06-06

- In progress. The network fingerprint no longer hashes raw float bits; it
  quantizes known hashed float fields through `sim::Scalar` / Fixed12.
- Remaining authoritative float storage is still broad. `Ent::pos`, `vel`,
  `acc`, `size`, `counter_a` through `counter_d`, and stage fluid state are the
  highest-priority simulation fields because they affect movement, contact,
  animation gates, world state, or lockstep fingerprints.
- Deferred risk: these fields are still simulated as float. The current
  quantized hash can prevent false cross-ISA mismatches from tiny float-bit
  differences, but it does not prevent two peers from crossing different branch
  thresholds before quantization. The follow-up migration is to move
  authoritative gameplay storage/math to fixed-point, integer counters, or
  explicit threshold quantization.
- Current authoritative float-backed inventory:
  `Ent::pos`, `vel`, `acc`, `size`, and `counter_a` through `counter_d`;
  stage fluid amount, velocity, gravity vectors, and temporary gravity;
  synchronized gameplay settings for fluids.
- Current lockstep hash behavior: entity position, velocity, acceleration,
  size, and counters are quantized through `sim::Scalar` / Fixed12 before
  hashing; entity rotation is stored and hashed as raw Fixed12, and
  distance-traveled, travel-sound countdown, support-ground-friction,
  push-acceleration, entity light state, animation time, animation scale,
  animation speed, stage gravity, and runtime effect instance values are also
  stored and hashed as raw Fixed12. Runtime movement tuning scalars `max_speed`,
  `throw_velocity_scale`, and `buoyancy` are stored and hashed as raw Fixed12
  too. That reduces noisy float-bit hash mismatches, but the simulation and
  `SimSnapshot` still carry IEEE float payloads for the remaining float-backed
  fields.
- Migration order should be narrow and mechanical: introduce fixed-point
  storage at authoritative boundaries, convert parsing/spec constants into
  fixed values, keep render/audio/UI conversion at the edge, and validate each
  subsystem against current gameplay feel before moving to the next. Entity
  `pos` / `vel` / `acc` / `size` and common physics/contact helpers are the
  first real migration target; stage fluids and gameplay settings are a second
  target because they are broader and more tuning-sensitive.
- Audit checkpoint 2026-06-07: generic entity counters are mixed-use, so they
  are not safe to convert wholesale. Most `counter_a` through `counter_d` uses
  are integer frame countdowns, small enum/state encodings, ammo/value counts,
  or cooldown flags, but some counters are distance accumulators driven by
  `dist_traveled_this_frame` for smoke/pebble/trail intervals. The follow-up
  migration should split these generic float counters into typed per-entity
  integer frame counters, explicit enum fields, and fixed/quantized distance
  accumulators rather than changing the four shared fields in one pass.
- Audit checkpoint 2026-06-07: join-accept bootstrap spawn coordinates now use
  Fixed12 (`sim::Scalar`) on the packet struct and wire. The network protocol
  version is `5`. The peer still converts these values back to `Vec2` at the
  current spawn API boundary, and the authoritative join barrier snapshot still
  decides final synchronized topology, but this removes raw IEEE float payloads
  from the initial synchronized room-join handoff.
- Follow-up cleanup: the generic packet `float` array read/write helpers were
  removed after the join-accept migration. New protocol fields should use
  explicit integer/fixed encodings unless there is a documented reason to send
  presentation-only IEEE float data.

## Math Function Audit

- [ ] Find gameplay uses of `std::sin`, `std::cos`, `std::tan`, `std::atan2`,
      `std::sqrt`, `std::hypot`, `std::pow`, `std::fmod`, `std::round`,
      `std::floor`, and `std::ceil`.
- [ ] Decide which uses can remain render-only.
- [ ] Replace gameplay-affecting trig with deterministic tables, discrete
      direction vectors, or fixed/integer approximations.
- [ ] Replace gameplay-affecting length/normalize code with deterministic
      alternatives or avoid normalization in authoritative state.

### Status 2026-06-06

- In progress. Initial search found gameplay math calls in:
  `world_query.cpp`, `ents/common/physics.cpp`, `ents/common/hang.cpp`,
  `ents/moving_platform.cpp`, `ents/ball_and_chain.cpp`, `ents/arrow_trap.cpp`,
  `ents/bow.cpp`, `ents/web_cannon.cpp`, `ents/bomb.cpp`, and related entity
  logic.
- Clear render/cosmetic math exists in lighting, acoustics, render shake, and
  particle paths; those can remain float unless they feed back into gameplay.
- Fixed in this pass: the initial high-risk libm hits in circular moving
  platforms and ball-and-chain were converted below. The remaining libm search
  results are now classified as render/audio/debug/cosmetic presentation
  boundaries, with the larger remaining gameplay risk being float-backed
  authoritative storage rather than direct platform-libm calls.
- Fixed for circular moving platforms: runtime `std::sin` / `std::cos` were
  replaced with a fixed 80-step integer unit-circle table. The platform still
  writes float `pos` / `vel` for the current physics pipeline, but its path
  selection and pixel offset generation no longer depend on platform libm.
- Fixed for ball-and-chain: runtime `std::sqrt` was replaced with fixed-scale
  integer length/direction math for catchup, taut-chain pull, and player
  pullback. The entity still reads/writes float `pos`, `vel`, and `acc` at the
  current physics boundary, but the pull direction no longer depends on
  platform libm.
- Fixed in tile queries: `QueryTilesInWorldRect` now uses integer floor
  division for integer world-pixel bounds instead of converting to float and
  calling `std::floor`. This removes a float/libm boundary from broad tile
  collision and world geometry queries.
- Fixed in shared gameplay boundary helpers: world wrap deltas, spatial-index
  cells, shake tile lookup, blocking-contact tile probes, ground-friction
  support probes, climb/hang probes, mattock tile probes, web-cannon tile snap,
  meat-slime surface keys, and trap-block sensor distances now use local
  `FloorToInt` / `RoundToInt` helpers instead of platform libm
  `std::floor`, `std::round`, or `std::fmod`.
- Fixed in tile-shake propagation: area radius expansion now uses local
  `CeilToInt`, and per-tile radial falloff uses fixed-scale integer distance
  plus `IntegerSqrtFloor` instead of platform libm `std::ceil` / `std::sqrt`.
  Tile shake is presentation state, but it is snapshot/replay state, so this
  keeps local replay and resync bytes less platform-sensitive.
- Fixed in entity-shake propagation: per-entity radial falloff now uses
  fixed-scale integer distance plus `IntegerSqrtFloor` instead of platform
  libm `std::sqrt`. Entity shake is presentation state, but it is also
  serialized in local snapshots/replays, so this keeps that path less
  platform-sensitive.
- Fixed in gameplay snap-to-pixel boundaries: blocking-bottom grounding,
  detached carry placement, stage-rotation entity placement, arrow-trap stored
  offsets, bow arrow spawn centers, sticky-bomb entity attachment offsets, and
  teleporter holder destinations now use local `RoundToInt` instead of platform
  libm `std::round` / `std::lround`.
- Fixed in gameplay branch/timer conversions: climb probe pixel limits, player
  fall-timer rate increments, and craps dice roll resolution now use local
  `CeilToInt` / `RoundToInt` instead of platform libm `std::ceil` /
  `std::round`.
- Fixed in 8-way weapon aim: bow and web-cannon projectile aim now use explicit
  discrete direction vectors and angle lookup instead of runtime
  `NormalizeOrZero` / `std::atan2`. This removes libm `sqrt` / `atan2` from
  player-controlled projectile velocity and stored weapon rotation.
- Fixed in gameplay distance thresholds: arrow-trap movement detection, arrow
  rotation velocity gating, piranha target/bite range checks, projectile-contact
  activation, spike high-speed override checks, monkey sight checks, spider
  aggro/drop checks, and cobweb occupant movement checks now compare squared
  lengths instead of calling `Length` / platform libm `std::sqrt`.
- Fixed in stage-generation distance thresholds: branch-exit placement,
  spawn-overlap checks, key/chest spacing, mines treasure/ambient spawn
  spacing, and arrow-trap entrance spacing now compare squared lengths. The
  shared nearest-spawn helper now returns squared distance so callers do not
  reintroduce a `sqrt` boundary for radius checks.
- Fixed in snapshot-preserved presentation branches: baseball-bat trail
  distance gating now compares squared length, and teleporter telefrag split /
  merge effect axes now use explicit cardinal/diagonal direction vectors
  instead of normalizing through `Length`. These effects are cosmetic for
  network lockstep, but they are present in local snapshots/replays.
- Fixed in remaining snapshot-preserved presentation vector boundaries:
  scripted teleporter phase effects now use deterministic normalization for
  their discrete axis helper, baseball-bat trail emission uses squared distance
  in `pres_commands.cpp`, and web-cannon spray particles use
  `NormalizeOrZeroDeterministic`. These are still presentation effects, but
  they no longer depend on platform libm `sqrt` when captured in local
  debug snapshots/replays.
- Fixed in snapshot-preserved stage-acoustics cache math: openness ray scoring
  no longer calls `std::sqrt(2)` because the diagonal step distance canceled
  out of the final ratio. `StageAcoustics` is captured by full local
  `GameplaySnapshot` for debug playback, but not by network `SimSnapshot`, so
  this cleanup protects local replay bytes without changing lockstep state.
- Deferred risk: those helpers make the float-to-integer conversion rule
  explicit, but the source positions are still authoritative floats. Two peers
  can still diverge if prior float math crosses a grid/branch threshold
  differently. The fixed-point migration must move these source values or their
  threshold decisions to fixed/integer state.
- Fixed in authoritative entity rotation: `Ent::rotation` is now stored as
  Fixed12 state instead of a raw float, and live network lockstep hashes add the
  raw fixed value directly. Held weapons still use it for discrete aim pose,
  rolling/spinning entities quantize velocity-derived visual rotation at the
  assignment edge, and rendering/debug/particle systems convert to float only at
  presentation boundaries. Recording format version is now 97.
- Audit checkpoint 2026-06-07: verified and re-aligned the live network
  fingerprint boundary for `Ent::rotation`. Full canonical fingerprints and
  snapshots already included the Fixed12 field; `AddNetworkEntFingerprint` now
  includes the raw fixed rotation for both normal entities and player/held
  entities whose motion is otherwise prediction-ignored.
- Deferred risk: some rotation writers still derive their input from
  authoritative float velocity or libm angle calculations before quantizing to
  Fixed12. This pass prevents rotation itself from carrying arbitrary IEEE
  payloads through hashes/snapshots; the broader movement/velocity migration
  must still remove the source float branch risks.
- Fixed in authoritative travel-distance state: `Ent::dist_traveled_this_frame`
  and `Ent::travel_sound_countdown` are now stored as Fixed12, hashed as raw
  fixed values, and recorded as raw fixed values. Physics still derives the
  per-frame traveled distance from float `pos` until the larger movement storage
  migration, but the distance thresholds, sound countdowns, and animation gates
  no longer carry raw float state between frames. Recording format version is
  now 98.
- Fixed in entity movement tuning state: `Ent::support_ground_friction` and
  `Ent::push_acc` are now stored as Fixed12, hashed as raw fixed values, and
  recorded as raw fixed values. Spec-authored constants still enter through
  float data-loading boundaries, and current physics friction/push acceleration
  converts back to float until the broader `pos` / `vel` / `acc` migration.
  Recording format version is now 99.
- Fixed in snapshot-preserved entity light state: `Ent::self_light`,
  `Ent::light_strength`, and `Ent::light_color` are now stored as Fixed12,
  hashed as raw fixed values, and recorded as raw fixed values. These fields are
  presentation/lighting state rather than gameplay authority, but they are part
  of canonical local fingerprints and debug playback snapshots, so they should
  not carry arbitrary platform float payloads. Spec-authored constants and
  render/stage-lighting code still use float `Color3` at their boundaries.
  Recording format version is now 100.
- Fixed in snapshot-preserved entity presentation state: `Ent::alpha` and
  `Ent::shake` are now stored and recorded as Fixed12. These fields affect
  render opacity and visual shake, not gameplay authority, but they are
  captured by debug playback snapshots, so fixed storage removes another pair
  of arbitrary platform float payloads from replay state. Spec-authored alpha
  and render/debug consumers still convert through float boundaries. Recording
  format version is now 101.
- Fixed in gameplay movement tuning state: `Ent::max_speed`,
  `Ent::throw_velocity_scale`, and `Ent::buoyancy` are now stored as Fixed12,
  hashed as raw fixed values in both canonical and network fingerprints, and
  recorded as raw fixed values. Current physics/throw code still converts these
  values to float at the existing `pos` / `vel` / `acc` boundary until the
  larger movement migration, but the runtime tuning state itself no longer
  carries arbitrary platform float payloads. Recording format version is now
  102.
- Fixed in runtime animation state: `AFrameAnimator::current_time`, `scale`,
  and `speed` are now stored as Fixed12, hashed as raw fixed values, and
  recorded as raw fixed values. Animation frame gates now compare fixed
  animation time against integer-authored frame durations, while rendering,
  debug JSON, CLI diffs, and text export convert back to float only at their
  presentation boundaries. Recording format version is now 103.
- Fixed in stage gravity state: `Stage::gravity` is now stored as Fixed12,
  hashed as a raw fixed value, and recorded as a raw fixed value. Current
  entity physics consumers convert it to float at the existing movement
  boundary until the broader `pos` / `vel` / `acc` migration. Recording format
  version is now 104.
- Fixed in runtime effect instance state: `EffectInstance::value` and retained
  reconnect effect mirrors are now stored as Fixed12, hashed as raw fixed
  values, and recorded as raw fixed values. Authored `EffectModifier` values and
  synchronized gameplay settings still enter as float data/tuning boundaries;
  the runtime effect payload itself no longer carries arbitrary float state.
  Recording format version is now 105.
- Fixed in authored effect modifier values: `EffectModifier::value` is now
  stored as Fixed12 and modifier application runs in fixed-point before
  returning to the current float-backed gameplay APIs. Item/passive modifier
  specs no longer retain raw float payloads. Runtime water-effect overrides
  still read synchronized float settings and quantize them at the effect
  boundary; moving those settings themselves to fixed remains part of the
  synchronized gameplay settings pass.
- Fixed in synchronized water-effect tuning: `WaterEffectSettings` gameplay
  values are now stored as Fixed12, settings-file parsing/UI editing convert at
  the local presentation boundary, and debug playback records them as raw fixed
  values. These settings feed the runtime `InWater` effect modifiers for
  gravity, damping, movement speed, buoyancy, fall damage, stomp behavior, and
  swim impulse, so they no longer preserve raw float payloads before the effect
  boundary. Fluid simulation settings remain a float-backed gameplay settings
  bucket for later passes. Recording format version is now 113.
- Fixed in synchronized player tuning: float-valued `PlayerTuningState`
  gameplay settings are now stored as Fixed12, settings-file parsing/UI editing
  convert at local presentation boundaries, and debug playback records them as
  raw fixed values. The current player control/physics helpers still accept
  floats, so tuning converts back at the existing player boundary until the
  broader player movement and `Ent::pos` / `vel` / `acc` migration. Recording
  format version is now 114.
- Fixed in synchronized fluid settings: float-valued `FluidSettings` scalar
  knobs are now stored as Fixed12, settings-file parsing/UI editing convert at
  local presentation boundaries, and debug playback records them as raw fixed
  values. The current fluid simulation and render paths still operate on floats,
  so settings convert back at those existing boundaries until the broader stage
  fluid-grid migration. Recording format version is now 115.
- Fixed in stale entity scalar state: unused `Ent::attack_weight` and
  `Ent::weight` were removed instead of converted. A current code/data scan
  found no gameplay readers and no authored data writers; the fields were only
  reset and serialized through debug playback snapshots. Removing them deletes
  two dead float payloads from entity state and bumps the recording format to
  version 106.
- Fixed in generic entity threshold state: `Ent::threshold_a` and
  `Ent::threshold_b` are now stored as Fixed12, recorded as raw fixed values,
  and included in canonical plus live network fingerprints. Current live
  gameplay uses are discrete sign/flag/radius/cooldown values for doors,
  trap blocks, moving platforms, and monkeys; the debug moving-light stress
  entity converts them to float only at its debug presentation boundary.
  Recording format version is now 107.
- Fixed in authored entity spec boundaries: spec-authored values for
  support-ground friction, push acceleration, throw velocity scale, buoyancy,
  alpha, self light, light strength, and light color are now stored as Fixed12
  on `EntSpec`. Entity spawn/restore copies these fixed values directly instead
  of requantizing raw float spec payloads every time an entity changes type.
  Authored C++ literals still enter through explicit `sim::ToSimScalar` /
  `sim::ToSimColor3` calls at spec construction, but the runtime spec table no
  longer keeps duplicate raw float copies for fields whose live entity state is
  already fixed-point.
- Fixed in stage-rotation snapshot state: `StageRotationState::pivot` is now
  stored as Fixed12 and recorded as raw fixed values. The pivot is derived from
  integer stage pixel dimensions and only converted back to `Vec2` at the
  renderer boundary. Stage rotation is presentation-oriented, but it is present
  in both gameplay and sim snapshots, so this removes another raw IEEE `Vec2`
  payload from rollback/resync/debug recording state. Recording format version
  is now 108.
- Fixed in network sim snapshot timing state: `SimSnapshot` no longer stores or
  restores `State::now` or `State::time_since_last_update`. Those fields belong
  to the local outer-frame scheduler, not deterministic fixed-tick simulation,
  and copying them through rollback/resync snapshots could import another
  machine's wall-clock accumulator. Full local `GameplaySnapshot` still records
  them for debug playback where local frame reconstruction is expected.
  Recording format version is now 109.
- Fixed in retained reconnect topology state: retained player last position and
  retained attached item position, velocity, acceleration, size, and generic
  counters are now stored as Fixed12 mirrors. Capture quantizes from the current
  float-backed entity pipeline, and reconnect restore converts back at the
  entity-spawn boundary. This does not replace the broader entity movement
  migration, but it prevents delayed reconnect/topology state from preserving
  arbitrary IEEE float payloads after a player disconnects.
- Fixed in snapshot-preserved tile shake state: `Stage::tile_shake` and
  `Stage::backwall_tile_shake` are now stored as Fixed12 grids and recorded as
  raw fixed values. Tile shake remains presentation state and the public stage
  API still accepts/returns float at render/caller boundaries, but local replay,
  gameplay snapshots, and stage wrap/rotation transforms no longer carry raw
  IEEE float payloads for this presentation state. Recording format version is
  now 110.
- Fixed in snapshot-preserved fluid presentation state:
  `Stage::fluid_display_amount` is now stored as a Fixed12 grid and recorded as
  raw fixed values. This field is render smoothing/presentation state; the
  authoritative fluid simulation still uses `fluid_amount`, `fluid_velocity`,
  gravity overrides, and gameplay fluid settings, which remain deferred for the
  broader fluid determinism pass. Recording format version is now 111.
- Fixed in fluid gravity override activation state:
  `Stage::fluid_gravity_strength` was actually a binary override-active flag,
  only written as `1.0F` / `0.0F` and read as `> 0`. It is now stored as an
  explicit `uint8_t` grid and recorded as bytes. The override gravity vectors
  and temporary gravity vectors remain float-backed fluid simulation state for
  the broader fluid pass. Recording format version is now 112.
- Fixed in runtime fingerprints: `FingerprintWriter::AddPod` no longer feeds
  raw host scalar memory into FNV. Integer, enum, bool, and fixed-point raw
  values now hash as explicit little-endian bytes. `AddPod` now rejects
  `float`/`double` at compile time so future fingerprint additions must
  explicitly quantize simulation scalars before hashing instead of silently
  introducing raw IEEE float payloads. This removes host byte order from
  canonical/gameplay/network fingerprints and guards the hash boundary against
  accidental float-bit comparisons.
- Fixed in runtime fingerprints: stage type now hashes as an explicit
  `uint8_t` instead of a plain `int` cast. This keeps the fingerprint enum
  boundary aligned with the snapshot writer's one-byte high-level mode/type
  encodings.
- Fixed in vendored fixed-point boundary conversion: `gfxp::BasicFixed::
  from_float_for_boundary` now uses local deterministic floor/ceil/nearest
  helpers instead of platform libm `std::floor`, `std::ceil`, and `std::round`.
  This keeps float-to-Fixed12 quantization explicit for fingerprints and other
  current fixed-point boundary crossings.
- Fixed in gameplay normalization helpers: bat chase, ghost-ball chase,
  piranha chase, and explosion knockback now use
  `NormalizeOrZeroDeterministic`, which quantizes the source vector to a
  Fixed12-scale integer grid, computes length with `IntegerSqrtFloor`, and
  returns a quantized unit vector without platform libm `sqrt`. These paths
  still write float acceleration/knockback into the current physics pipeline,
  but their direction choice no longer depends on cross-platform square-root
  behavior.
- Fixed in world-query raycast stepping: general tile/entity/world raycasts now
  use `NormalizeOrZeroDeterministic` for non-axis-aligned directions. Existing
  horizontal and vertical raycasts were already integer-stepped; this removes
  platform libm `sqrt` from the remaining arbitrary-direction world query
  stepping while the query result still returns current integer pixel samples.
- Fixed in physics traveled-distance bookkeeping: `Ent::dist_traveled_this_frame`
  now uses `LengthDeterministic` instead of platform libm `sqrt`. This field
  feeds gameplay-adjacent timers, sound countdowns, boulder counters, and player
  animation gates, so its per-frame distance threshold is now quantized through
  the same Fixed12-scale integer length path as deterministic normalization.
- Fixed in arrow rotation bookkeeping: falling/flying arrow rotation now
  quantizes velocity to the Fixed12 grid and uses an integer-only atan
  approximation before storing `Ent::rotation`. Arrow velocity is still
  float-backed until the broader movement migration, but this authoritative
  fixed rotation writer no longer depends on platform libm `std::atan2`.
- Fixed in snapshot-preserved audio acoustics: listener/source distance tests
  and occlusion ray lengths now use `LengthDeterministic` and `CeilToInt`
  instead of platform libm `std::sqrt` / `std::ceil`. Positional audio
  acoustics are outside live network lockstep, but `StageAcoustics` is captured
  by local gameplay snapshots/debug playback, so this keeps that replay boundary
  under explicit deterministic rounding.
- Fixed in fluid vector math boundaries: `stage_fluids.cpp` now uses
  `LengthDeterministic` for velocity clamping and gravity magnitude, and
  `NormalizeOrZeroDeterministic` for gravity and neighbor transfer directions.
  Fluid amounts and velocities are still stored as floats, but the remaining
  simple cleanup pass no longer has an authoritative fluid path calling
  platform libm `sqrt` through `Length` / `NormalizeOrZero`.
- Remaining high-risk math boundary after the simple cleanup pass: broad
  authoritative gameplay storage is still float-backed, including fluid
  amounts/velocities and entity position/velocity/acceleration. These need the
  larger fixed-point storage migration or explicit per-system quantization
  policy covered by the Gameplay Float Audit, rather than more isolated libm
  call replacement.
- Audited remaining `stage_lighting.cpp` libm use: `std::pow`, `std::floor`,
  and render-time `Length` feed the cached lighting grids and sampling helpers.
  The cache is not included in live network `SimSnapshot` bytes or lockstep
  hashes. It is copied by full in-memory `GameplaySnapshot` and rollback
  presentation state, but rollback invalidates the lighting cache after
  restoring presentation, and persisted debug recording IO currently does not
  write the live cache. Treat this as quarantined presentation math unless
  lighting cache values later feed gameplay branches or transmitted state.
- Audited remaining `debug_moving_light.cpp` libm use: the debug lighting stress
  entity writes position/rotation from `std::sin` / `std::cos`, but the type is
  spawned only by the debug lighting stress tool. It can desync a debug stress
  stage across ISAs if that stage is ever used as an authoritative multiplayer
  test, but it is not part of normal quest gameplay. Leave it as a documented
  debug-only boundary unless the stress entity is promoted into live content.
- Remaining mostly-cosmetic math boundaries: particle/audio/render/debug
  rendering paths still use libm for presentation outside live network lockstep
  hashes and outside transmitted `SimSnapshot` bytes. Cross-platform debug
  playback can still differ cosmetically until presentation state gets its own
  deterministic or explicitly host-local policy.

## Container And Iteration Order Audit

- [ ] Search deterministic simulation code for unordered containers.
- [ ] Ensure entity iteration order is stable.
- [ ] Ensure contact/collision pair ordering is stable when multiple candidates
      tie.
- [ ] Ensure maps keyed by pointers, addresses, or allocation order do not affect
      gameplay results.
- [ ] Avoid sorting by non-stable values or platform-dependent comparisons.

### Status 2026-06-06

- In progress. Network entity fingerprints already sort active entities by
  stable network entity id, falling back to VID. Stage lights are sorted by VID
  before hashing.
- `Sid` uses `std::unordered_map`, but it is an index/query cache keyed by
  integer bucket and query traversal is by explicit coordinate loops, not
  unordered-map iteration. Fixed: `SID::Query` now sorts returned VIDs by
  `id` / `version` before callers consume the result, so per-bucket insertion
  order cannot leak into collision/contact ordering.
- Fixed in shared world queries: `QueryEntsInAabb` now re-sorts the final
  deduplicated result by VID after merging wrapped-world sample offsets. This
  keeps wrapped-stage broadphase consumers from inheriting offset enumeration
  order when the same query touches multiple wrapped copies.
- Fixed in moving-platform carry ordering: riders with equal carried x
  positions now use VID as a deterministic tie-breaker. The old comparator left
  equal-position riders equivalent under `std::sort`, so their displacement
  order was not explicitly defined.
- Fixed in blocking tile-contact ordering: `GatherBlockingContactsForAabb`
  now sorts gathered tile contacts by wrapped tile coordinate before callers
  resolve collision, tile contact callbacks, and collision sounds. The query
  loops were already deterministic, but wrapped-stage queries could otherwise
  expose first-seen wrap order to contact handling instead of a canonical tile
  order.
- Fixed in fingerprint diagnostic/network ordering: stage lights now sort by
  full VID id/version instead of id alone, and network tool/entity fingerprint
  sorters now fall back to full VID id/version when network ids tie. This keeps
  fingerprint byte order strict even across entity id reuse/generation cases.
- Fixed in asset database construction: `AFrameDb::FromRaw` now sorts grouped
  animation names before appending to `database.anims`. Animation ids were
  already hashed from names, but the backing animation vector and index maps no
  longer inherit `unordered_map` iteration order across standard libraries.
- Fixed in classic stage construction: quest stage exits are now copied from
  the parsed exit map into `Stage::exits` in sorted exit-id order. `StageExitId`
  is the vector index returned by `Stage::FindExitId`, so this removes
  `unordered_map` iteration order from exit id assignment and stage-transition
  routing.
- Fixed in buyable overlap selection: overlapping shop items now sort by a
  Fixed12-quantized squared distance and then stable entity id instead of
  comparing raw float squared distances. The selected overlap controls both the
  buy prompt and which item `TryBuyEnt` targets, so this branch now uses the
  same position quantization grid as lockstep fingerprints while the broader
  entity-position migration remains open.
- Audit checkpoint 2026-06-07: a current `unordered_map` / `unordered_set` /
  structured-binding iteration scan found no remaining high-confidence live
  gameplay iteration order leak. The remaining unordered containers are
  currently lookup/index caches (`Sid`, content-name maps, audio/animation
  lookup maps, quest/spec lookup maps, render caches) or validation/debug/CLI
  data. The one remaining gameplay-tree structured binding in
  `ents/common/hang.cpp` iterates a fixed `std::array` of debug probe labels
  only. Existing gameplay sorters checked in this pass use stable VID,
  coordinate, id, or sequence tie-breakers where the order can affect state.
- Deferred risk: continue auditing contact/collision tie ordering outside the
  common broadphase and moving-platform carry paths.

## RNG Audit

- [x] Identify every RNG stream.
- [x] Separate deterministic gameplay RNG from visual/debug/UI randomness.
- [x] Ensure all gameplay RNG is seeded from synchronized state.
- [x] Ensure joining peers receive exact RNG state in snapshots.
- [x] Remove or quarantine process-global RNG use from gameplay.

### Status 2026-06-06

- Partially fixed. `rng::Random*` no longer depends on
  `std::uniform_int_distribution`, `std::uniform_real_distribution`, or
  `std::mt19937` mapping. It now uses the local `DetRng` algorithm for stable
  integer and float mapping after seeding.
- Rule recorded: authoritative gameplay randomness must use `state.drng` or
  another synchronized `DetRng` stream. The `rng::` namespace is process-global
  and remains appropriate only for render, audio variation, debug bots, local
  presentation particles, and initial local seed creation before host state is
  synchronized.
- Initial search found process-global `rng::` calls in many particle/audio
  feedback paths and a small set of gameplay-adjacent paths:
  `stage_progression.cpp` local run seed creation, `ents/bat.cpp` sound choice,
  `ents/baseball_bat.cpp` kill sound choice, `ents/meathead.cpp` popup
  placement/flip, and debug input bot code. The sound/debug paths are
  non-authoritative. Meathead popup particles are not in `SimSnapshot`, but
  should stay quarantined as presentation-only.
- Fixed: meathead popup tile choice and flip now use synchronized `state.drng`.
  The chosen popup center is also used for world sound emitters, and audio
  emitters are part of rollback/resync snapshots, so this cannot safely use the
  process-global presentation RNG.
- Fixed: bat pursuit squeak/flap selection and baseball-bat kill sound
  selection now use synchronized `state.drng`. These choices create audio
  emitters, and audio emitters are part of rollback/resync snapshots.
- Audited process-global `rng::` call classes. The remaining broad set in
  entity particle bursts, render shake, presentation commands, treasure pickup
  effects, debug stage builders, and debug input bots is presentation/debug
  randomness. These values are not part of `SimSnapshot` and do not participate
  in lockstep hashes or network resync. They can remain process-local unless
  they are later promoted into authoritative sim state.
- Quarantined boundary: full local `GameplaySnapshot` still includes particles,
  audio emitters, and stage lighting so local recording/playback can restore
  presentation state. Process-global presentation RNG therefore remains
  acceptable for lockstep determinism, but full debug recordings are not
  promised to be presentation-identical across platforms unless these effects
  are given deterministic presentation streams.
- Quarantined boundary: `MakeRandomStageSeed()` intentionally uses
  process-global `rng::RandomU32()` only to create a fresh host/local run seed
  before the new stage is synchronized. The chosen seed is then copied into
  `state.net_session.stage_seed`, transition packets, snapshots, and stage
  generation state. Peers must never independently call it for the same network
  transition.
- Deferred risk: keep auditing future `rng::` additions. Any process-global RNG
  that creates audio emitters, mutates `SimSnapshot`, changes entity/world
  state, or affects synchronized branch decisions must move to `state.drng` or
  another synchronized `DetRng` stream.
- Removed stale process-global direction helpers: `Side`, `DownOrUp`,
  `SideOrDown`, and `RandomDirection` in `direction.cpp` had no callers and
  returned gameplay-shaped values from process-global RNG. `Direction` remains
  as a plain enum, but the dead RNG helper entry points are gone instead of
  being left as a future deterministic-gameplay footgun.
- Validation 2026-06-07: release build, `--check-state-equality-smoke`,
  `--check-det-replay-smoke`, and `--check-join-barrier-next-stage-restart-smoke`
  passed after removing the stale process-global direction helpers.
- Removed stale sprite animation randomization:
  `SpriteAnimator::RandomizeFrame` had no callers and used process-global RNG
  on a type that can sit on entity state. If randomized animation phase is
  reintroduced for gameplay-visible or snapshot-visible animation later, it
  must accept a synchronized `DetRng&`; presentation-only randomization should
  live outside authoritative entity state.
- Audit checkpoint 2026-06-07: a current `rng::Random*` scan found the
  remaining process-global RNG call sites are the quarantined fresh run seed,
  presentation effects/render shake/audio-visual particles, debug input-bot
  generation, and debug stage decoration. No remaining call site was identified
  that directly mutates synchronized world/entity/player state. Debug input-bot
  output can still drive gameplay by becoming local input, so portable replay
  must rely on captured input records rather than re-running the bot RNG.

## Serialization And Snapshot Audit

- [ ] Ensure deterministic snapshots use explicit integer sizes and endian rules.
- [ ] Avoid serializing `size_t`, `long`, pointers, enum storage assumptions, or
      padding bytes.
- [ ] Ensure all gameplay fields that affect simulation are included in
      snapshots/resync state.
- [ ] Ensure desync replay files include enough final local state to diff entity
      fields, not just hashes.

### Status 2026-06-06

- In progress. Snapshot vector, grid, and string counts are now serialized with
  explicit fixed-width counts at the current shared snapshot/replay boundary.
- Fixed in fingerprints: `FingerprintWriter` no longer hashes `size_t` values
  for strings/vector counts. Counts now enter hashes as explicit `uint64_t`.
- Fixed in snapshots: entity runtime callback function pointers are no longer
  serialized. They are runtime metadata, not authoritative state, and
  `RestoreGameplaySnapshot` / `RestoreSimSnapshot` rebind callbacks from the
  local entity specs after restore.
- Fixed in debug recordings: `GameplaySnapshot` video menu target indices are
  serialized as optional `uint32_t` values instead of raw
  `std::optional<std::size_t>`. Recording format version is now 73.
- Fixed in shared gameplay/simulation snapshot serialization: entity
  `stage_spawn_index` and `EntPool::available_ids` are now encoded as explicit
  optional/vector `uint32_t` values instead of raw host `size_t`. These fields
  affect spawn identity and entity id reuse after replay/resync. Recording
  format version is now 74.
- Fixed in shared gameplay/simulation snapshot serialization: `EntSpawn`
  linked spawn indices and `Stage::next_light_vid` now use explicit
  optional/single `uint32_t` values instead of raw host `size_t`. Recording
  format version is now 75.
- Fixed in network fingerprints and shared gameplay/simulation snapshot
  serialization: `AFrameAnimator::current_frame` is encoded as an explicit
  `uint32_t` instead of raw host `size_t`. Recording format version is now 76.
- Fixed in runtime fingerprints and shared gameplay/simulation snapshot
  serialization: `VID::id` now uses explicit `uint32_t` storage instead of raw
  host `size_t`. Entity, audio-emitter, audio-instance, light, optional VID, and
  vector-of-VID snapshot fields no longer carry platform-width ids by raw
  struct layout. Recording format version is now 77.
- Fixed in canonical gameplay/replay fingerprints: `VID` hashing now includes
  both `id` and `version`. Entity pool version is part of stale-reference
  identity, so canonical hashes must distinguish a live reference from a reused
  id with a different generation.
- Fixed in shared gameplay/simulation snapshot serialization: VID-bearing
  fields now use explicit `id` / `version` writers instead of generic raw POD
  helpers. This covers entity VIDs, optional/vector VID links, stage lights,
  player-slot entity VIDs, contact cooldown/dispatch VIDs, tool owner VIDs,
  interact/area listener VID vectors, and net entity link local VIDs. Recording
  format version is now 78.
- Fixed in shared gameplay/simulation snapshot serialization: optional presence
  flags now use explicit `uint8_t` bytes instead of raw host `bool`
  representation. Recording format version is now 79.
- Fixed in shared gameplay/simulation snapshot serialization:
  `EffectInstance` and `ToolSlot` now encode enum ids and bool flags as
  explicit `uint8_t` values plus fixed-width scalar fields instead of raw host
  struct layout. Invalid effect/tool ids now fail snapshot decode. Recording
  format version is now 80.
- Fixed in shared gameplay/simulation snapshot serialization:
  `AFrameAnimator` now encodes animation bools and playback mode as explicit
  `uint8_t` fields instead of raw host `bool` / enum representation. Invalid
  playback modes now fail snapshot decode. Recording format version is now 81.
- Fixed in shared gameplay/simulation snapshot serialization: player topology
  fields now encode `PlayerConnectionKind`, player connected/primary flags,
  simulation player connected flags, and network entity input-owner presence as
  explicit `uint8_t` values instead of raw host `bool` / enum representation.
  Invalid player connection kinds now fail snapshot decode. Recording format
  version is now 82.
- Fixed in shared gameplay/simulation snapshot serialization: remaining
  high-level settings/stage/run-state booleans now use explicit `uint8_t`
  values. This covers serialized video/post-process bools, stage exit
  requirement expectations, spawn buyable flags, stage wrap/clamp flags,
  optional fullscreen targets, gameplay/simulation running and game-over/pause/
  win flags. Recording format version is now 83.
- Fixed in shared gameplay/simulation snapshot serialization: high-level mode
  and type enums now use explicit `uint8_t` values with decode validation
  instead of raw host enum storage. This covers game/menu modes, menu return
  modes, multiplayer respawn mode, settings mode, post-process effect, and
  stage type. Recording format version is now 84.
- Fixed in shared gameplay/simulation snapshot serialization: entity
  `Buyable`, `UseState`, and `AttachMode` are now encoded field-by-field
  instead of as raw host structs. Buy callbacks are encoded as explicit
  callback ids for the known buy handlers (`TryBuyEntForMoney` and
  `BuyDamsel`) instead of process-local function pointer bytes; an unmapped
  future buy callback intentionally fails decode until it is added to the
  mapping. Recording format version is now 85.
- Fixed in shared gameplay/simulation snapshot serialization: common entity
  booleans and small enums now use explicit one-byte encodings instead of raw
  host `bool` / enum storage. This covers entity activity/collision/pickup/
  hang/contact flags plus `EntType`, `Side`, `DrawLayer`, `TravelSound`,
  `EntCondition`, `EntAiState`, `DamageType`, `DamageVuln`, `PointLabel`,
  `EntLabel`, `Alignment`, and optional enum fields. Snapshot optional
  presence markers now reject invalid nonzero values instead of accepting any
  byte as present. Recording format version is now 86.
- Fixed in shared gameplay/simulation snapshot serialization: authored entity
  spawn records, background stamps, and pending stage-transition targets are now
  encoded field-by-field instead of through raw optional/enum struct layout.
  This removes raw `std::optional<Vec2>`,
  `std::optional<StageTransitionTarget>`, `EntSpawn` enum fields, and
  `BackgroundStamp` enum storage from replay/resync bytes. Recording format
  version is now 87.
- Fixed in shared gameplay/simulation snapshot serialization: stage tile grids,
  border tiles, fluid tile grids, backwall tile grids, backwall fill tiles, and
  embedded treasure grids now encode tile/entity/visibility enum fields
  explicitly instead of depending on raw host enum struct layout. Stage
  annotation positions and stage-light tile positions are also written
  field-by-field. Recording format version is now 88.
- Fixed in shared gameplay/simulation snapshot serialization: the remaining
  stage bulk grid/vector fields now write their elements field-by-field instead
  of using raw vector storage. This covers fluid/shake float grids, fluid
  velocity/gravity `Vec2` grids, room id grids, stage path `IVec2` vectors,
  camera clamp margin, wrap padding, and wrap core `UVec2` values. Recording
  format version is now 89.
- Fixed in shared gameplay/simulation snapshot serialization: player and net
  entity topology ids now use explicit fixed-width writers instead of raw
  struct/vector layout. This covers player-registry ids, simulation player-slot
  ids, spectator target ids, network entity ids, network input-owner player ids,
  and network entity id aliases. Recording format version is now 90.
- Fixed in shared gameplay/simulation snapshot serialization: menu and playing
  input state now encodes button bools, input snapshots, input frames, debounce
  timers, and mouse coordinates field-by-field instead of through raw input
  struct layout. Recording format version is now 91.
- Fixed in shared gameplay/simulation snapshot serialization: `Settings` now
  round-trips field-by-field instead of serializing a partial raw-layout subset.
  This covers video resolution options, audio/acoustics config, controls, UI,
  post-process/stage-lighting config, fluid simulation config, water movement
  config, debug UI config, and player tuning. Recording format version is now
  92.
- Fixed in shared gameplay/simulation snapshot serialization: high-level run
  state now uses explicit field writers instead of raw structs for menu
  selections, debug overlay/brush state, stage rotation, player tuning, runtime
  RNG state, respawn targets, quest state, debug level config, and gameplay
  camera position. Stage-transition seeds now use an explicit optional
  `uint32_t`. Recording format version is now 93.
- Fixed in shared gameplay/simulation snapshot serialization: optional entity
  audio asset ids, optional entity stage-transition targets, stage generation
  seeds, and optional void-death boundaries now use explicit typed optional
  writers instead of generic raw optional payloads. Dead raw vector/optional/
  grid helper paths were removed from the recording writer. Recording format
  version is now 94.
- Fixed in shared gameplay/simulation snapshot serialization: top-level
  gameplay/simulation scalar fields now route through named fixed-width scalar
  writers instead of generic POD calls. This covers `now`,
  `time_since_last_update`, `scene_frame`, `frame`, `stage_frame`, points,
  deaths, depth, sac altar favor/reward tier, and `frame_pause`. Recording
  format version is now 95.
- Fixed in shared gameplay/simulation snapshot serialization: entity `Vec2`,
  `IVec2`, and `Color3` fields now use component writers instead of raw struct
  POD calls. This covers entity position, velocity, acceleration, size, light
  color, and generic entity point fields. The emitted field bytes are unchanged
  for the current structs, so this is a layout-dependency cleanup within
  recording format version 95.
- Fixed in shared gameplay/simulation snapshot serialization: remaining
  `AFrameAnimator` and `Ent` scalar fields now route through named fixed-width
  integer/float writers instead of raw member POD calls. This covers animator
  ids/timers/counts and entity timers, counters, light values, health,
  money, movement flags, damage amounts, and cooldowns. The emitted scalar
  bytes are unchanged for the current field types, so this remains within
  recording format version 95.
- Fixed in shared gameplay/simulation snapshot serialization: stage scalar
  fields and contact-bookkeeping expiry frames now use named fixed-width
  helpers instead of raw member POD calls. This covers stage level number,
  gravity, block animation id, tile-change generation, and cooldown expiry
  frames. The emitted scalar bytes are unchanged for the current field types,
  so this remains within recording format version 95.
- Fixed in shared gameplay/simulation snapshot serialization: remaining small
  snapshot structs and ids now use named typed helpers instead of raw member
  POD calls. This covers VID id/version fields, stage-load debug variants,
  buyable display counts, use-state frames, effect counts/values/timers,
  authored spawn animation/buy ids, stage light radius, embedded treasure
  counts/audio ids, background stamp animation ids, and tool-slot count/
  cooldown fields. The emitted scalar bytes are unchanged for the current field
  types, so this remains within recording format version 95.
- Fixed in shared gameplay/simulation snapshot serialization: tile and
  tile-rotation grid elements now use explicit `uint16_t` / `uint8_t` helpers
  instead of raw local POD variables. This is byte-compatible with recording
  format version 95 and keeps the authoritative tile grids independent of enum
  storage and alias typedef assumptions.
- Fixed in shared gameplay/simulation snapshot serialization: local presence
  bytes, enum bytes, fixed-width ids, vector/grid counts, stage/player/tool/
  topology counts, and recording header fields now route through named
  fixed-width helpers instead of direct POD calls. The only remaining raw POD
  calls in the recording writer are the centralized scalar helper internals,
  so this is a byte-compatible cleanup within recording format version 95.
- Fixed in shared gameplay/simulation snapshot serialization: the centralized
  scalar helper internals now emit integer values in explicit little-endian
  byte order and encode `float` / `double` through explicit IEEE bit copies
  instead of raw host-endian POD writes. Recording format version is now 96.
- Fixed in desync replay diagnostics: SDRP metadata, hash components, replayed
  input records, and captured local entity hashes now route through explicit
  little-endian `uint16_t` / `uint32_t` / `uint64_t` writer and reader helpers
  instead of a generic raw POD helper or host memory order. The current SDRP
  version remains byte-compatible on the little-endian developer machines used
  so far, but the diagnostic artifact contract is now explicit for
  cross-platform desync work.
- Fixed in UDP net protocol packets: packet headers and all lobby/lockstep/
  snapshot-resync/topology/restart packet payloads now encode fields
  explicitly in little-endian fixed-width form instead of raw-copying local C++
  structs. This removes packet padding, host byte order, raw enum layout, and
  `sizeof(struct)` from live multiplayer packet compatibility. Network
  protocol version was bumped to 2 at this checkpoint.
- Fixed in UDP net protocol codec internals: integer packet writer/reader
  byte-shift loops now use explicit `uint32_t` counters instead of
  implementation-width `unsigned int`, without changing packet bytes or
  protocol version.
- Fixed in UDP join-barrier topology packets: joined player spawn positions
  now cross the wire as Fixed12 raw `int32_t` components instead of IEEE float
  payloads. Hosts quantize at packet build time, peers convert back only at the
  spawn edge, and the network protocol version is now 4 so old clients reject
  cleanly.
- Audit checkpoint 2026-06-07: current `SimSnapshot` field scan confirms
  `MakeSimSnapshot`, `RestoreSimSnapshot`, `WriteSimSnapshot`, and
  `ReadSimSnapshot` cover the same simulation snapshot fields. Omitted `State`
  fields are currently local/presentation/cache boundaries rather than
  authoritative simulation bytes: particles, audio emitters, stage lighting,
  audio listener position, gameplay camera anchor, controlled/spectator local
  view state, world prompts, debug annotations, `SID`, transport runtime, and
  performance counters. Network rollback/resync preserves presentation state
  explicitly with `RollbackPresentationSnapshot`, while `RestoreSimSnapshot`
  rebuilds SID and clears prompts/debug annotations after authoritative state
  restore.
- Audit checkpoint 2026-06-07: a current raw serialization scan found raw
  `memcpy` / POD-style byte handling isolated to scalar helper implementations
  and fixed-size byte array copies. `SimSnapshot` itself is serialized through
  named field writers/readers rather than host struct layout.
- Fixed/guarded float byte boundaries: network packet float payloads, runtime
  fingerprint float/double payloads, and debug recording float/double payloads
  now assert IEC 559 / IEEE-754 scalar formats at compile time before copying
  bits into explicit little-endian integer encoders. This makes the remaining
  portability contract visible instead of silently accepting an incompatible
  platform.
- Quarantined boundary: `StageTileTrigger` carries runtime callback/debug
  pointer fields and is not serialized by the stage snapshot writer. Network
  resync currently preserves local tile triggers around `RestoreSimSnapshot`,
  then rebinds entity callbacks from local specs. If debug recordings need to
  become fully portable across process sessions, tile triggers should be
  encoded as explicit trigger ids plus deterministic payload fields rather than
  raw callbacks.
- Deferred risk: `SerializeSimSnapshotToBytes` / `DeserializeSimSnapshotFromBytes`
  now avoid raw struct layout, raw enum storage, raw bool representation, host
  endian scalar bytes, and padding. They still preserve current `float` /
  `double` bit patterns for remaining authoritative float fields, guarded by
  the explicit IEEE-754 compile-time contract above, so the snapshot format is
  portable as bytes but cannot by itself prevent gameplay divergence caused by
  cross-platform float arithmetic before snapshot/resync.
- Audited presentation snapshot boundary: live network `SimSnapshot` does not
  include particles, audio emitters, stage acoustics, or stage lighting. Rollback
  keeps a local `RollbackPresentationSnapshot` for particles, audio emitters,
  stage lighting, camera/listener state, and controlled/spectator ids only to
  preserve local presentation across rollback correction; it is not transmitted
  as authoritative lockstep state.

## Undefined And Uninitialized Behavior Audit

- [ ] Run sanitizers locally where practical.
- [ ] Audit constructors/default initialization for gameplay structs.
- [ ] Avoid reading padding/uninitialized bytes in hashes.
- [ ] Avoid signed overflow in deterministic code.
- [ ] Avoid shift undefined behavior.
- [ ] Avoid aliasing assumptions that can differ by compiler.

### Status 2026-06-07

- Added a repeatable local sanitizer build path:
  `SPLONKS_PRESET=asan ./scripts/build.sh`. This enables ASan/UBSan on
  Clang/GCC-style toolchains through the `SPLONKS_SANITIZERS` CMake option.
- First sanitizer build surfaced mixed-width byte-mask promotions in the
  recording writer, network packet encoder, and state fingerprint writer.
  These fixed-width byte boundaries now use width-explicit masks before
  narrowing to bytes.
- Validation so far: release build passed, ASan/UBSan build passed, release
  state-fingerprint and gameplay snapshot smokes passed, and the ASan/UBSan
  binary passed the state-fingerprint smoke with leak detection disabled for
  the short headless run.
- Validation 2026-06-07: ASan/UBSan build passed again after the authored
  spawn-index conversion. The sanitizer build exposed one remaining SDRP
  `uint16_t` byte-mask promotion warning in `WriteReplayUint16`; that helper now
  uses a width-explicit `uint16_t` mask. The ASan/UBSan binary then passed
  `--check-state-fingerprint-smoke` and `--check-state-equality-smoke` with
  leak detection disabled for short headless runs.
- Validation 2026-06-07: ASan/UBSan build passed after the direct
  join-barrier-protocol CLI smoke initialized runtime spec tables. The sanitizer
  binary passed `--check-state-equality-smoke`,
  `--check-state-fingerprint-smoke`,
  `--check-gameplay-snapshot-callback-rebind-smoke`,
  `--check-network-fresh-reload-ownership-smoke`,
  `--check-join-barrier-protocol-smoke`, and
  `--check-join-barrier-next-stage-restart-smoke` with leak detection disabled.
  The broad `--check-det-replay-smoke` sanitizer run was stopped after several
  minutes of CPU-bound execution without sanitizer output; keep that as a
  runtime/perf follow-up rather than treating broad replay sanitizer coverage as
  complete.
- Deferred risk: sanitizer execution is not yet broad enough to close this
  section. Run sanitized headless smokes, local play, and two-client lockstep
  sessions before treating uninitialized/undefined behavior risk as audited.

## Platform-Sized Type Audit

- [ ] Avoid `long`, `size_t`, pointer values, and address-derived order in
      deterministic gameplay state.
- [ ] Use explicit types: `int32_t`, `uint32_t`, `int64_t`, `uint64_t`.
- [ ] Audit hashes for platform-sized values.
- [ ] Audit save/snapshot/replay formats for platform-sized values.

### Status 2026-06-06

- Partially fixed. Gameplay/network fingerprints no longer include direct
  `size_t` count representations.
- Fixed in the local debug/playback recording format: `GameplaySnapshot`
  menu-selection indices still use `std::optional<std::size_t>` in runtime
  state because they index local option arrays, but the on-disk recording format
  now stores them as explicit optional `uint32_t` values.
- Fixed in shared gameplay/simulation snapshot serialization:
  `Ent::stage_spawn_index` and `EntPool::available_ids` still use
  `std::size_t` in runtime state because they index local arrays, but the
  snapshot byte format now stores them as explicit `uint32_t` values.
- Fixed in shared gameplay/simulation snapshot serialization: `Stage::next_light_vid`
  initially still used `std::size_t` in runtime state because it is an id
  counter, but the snapshot byte format stored it as an explicit `uint32_t`
  value before runtime storage was later converted.
- Fixed in fingerprints and shared gameplay/simulation snapshot serialization:
  `AFrameAnimator::current_frame` now uses explicit `uint32_t` runtime storage,
  and fingerprints and snapshot bytes store it as the same explicit width. Local
  animation-frame vector indexing casts only at the lookup boundary.
- Fixed in runtime deterministic state: `VID::id` now uses `uint32_t` rather
  than `std::size_t`. This removes a broad platform-sized value from entity,
  light, audio-emitter, and audio-instance hashes/snapshots without changing
  gameplay identity semantics.
- Fixed in runtime fingerprints: scalar hash inputs now pass through explicit
  little-endian byte encoders instead of host memory order, including the
  remaining fixed-width integer fields and fixed-point raw values.
- Fixed in runtime fingerprints: `StageType` now uses an explicit byte
  encoding instead of a platform-width `int` cast.
- Fixed in shared snapshot/replay format: VID-bearing vectors and structs now
  serialize VIDs field-by-field instead of relying on host raw layout for
  vector payloads or optional payloads.
- Fixed in shared snapshot/replay format: optional presence flags now use an
  explicit one-byte representation instead of raw `bool`.
- Fixed in shared snapshot/replay format: player ids remain `uint32_t` in
  runtime, but snapshot bytes now route them through explicit fixed-width
  writers, including optional spectator targets and network input-owner ids.
- Fixed in shared snapshot/replay format: input structs still contain runtime
  `bool` fields, but the serialized bytes now use explicit one-byte bool
  markers and fixed-width `UVec2` mouse coordinates.
- Fixed in shared snapshot/replay format: settings values now use explicit
  scalar/vector writers, including full `UVec2` resolution option vectors,
  `uint32_t` controls/debug ids, `int32_t` frame/count values, and one-byte
  bool markers.
- Fixed in shared snapshot/replay format: deterministic RNG state is written as
  an explicit `uint64_t`, stage-transition seeds as optional `uint32_t`, and
  debug/test configuration enums and counters as explicit byte/int fields.
- Fixed in shared snapshot/replay format: entity audio asset optionals, entity
  stage-transition target optionals, stage generation seed optionals, and
  stage void-death boundary optionals now use typed optional encoders rather
  than raw optional payloads.
- Fixed in shared snapshot/replay format: top-level frame/time/score/depth/
  altar/pause scalar fields now use named fixed-width helpers rather than raw
  POD snapshot calls.
- Fixed in shared snapshot/replay format: entity vector/color fields now write
  components explicitly, avoiding dependence on `Vec2` / `IVec2` / `Color3`
  struct padding or aggregate layout.
- Fixed in shared snapshot/replay format: entity and animator scalar fields now
  use typed scalar helpers, removing the remaining raw `Ent`/`AFrameAnimator`
  member POD calls from the recording writer.
- Fixed in runtime deterministic state: `Ent::stage_spawn_index`,
  `Stage::next_light_vid`, and `EntPool::available_ids` now use explicit
  `uint32_t` storage instead of `std::size_t`. These values are bounded
  gameplay ids rather than arbitrary host vector sizes, and their snapshot/
  replay byte representation was already explicit `uint32_t`, so this removes
  machine-width runtime state without changing recording format version 96.
- Fixed in snapshot-preserved presentation state: `AudioEmitterManager::
  available_ids` now uses explicit `uint32_t` storage instead of
  `std::size_t`. Audio emitters are outside live network `SimSnapshot`
  authority, but they are copied through rollback/presentation snapshots and
  local debug playback state. The emitter pool is capped at 256 ids, so the
  free-list now matches `VID::id` width and only casts at local vector-index
  boundaries.
- Fixed in snapshot-preserved presentation state: ribbon and segmented-sprite
  particle `point_count` fields now use explicit `uint32_t` storage instead of
  `std::size_t`. These particles are not live network authority, but they are
  copied by local debug/presentation snapshots, and their point arrays are
  bounded at 32 and 64 entries.
- Fixed in local menu snapshot state: pending video-settings resolution/window
  size indices now use `std::optional<uint32_t>` at runtime instead of
  `std::optional<std::size_t>`. The debug recording format already stored these
  as optional `uint32_t`; this removes the host-width runtime field and keeps
  casts at the local `kResolutions` array-index boundary.
- Fixed in local performance telemetry: `PerformanceStats::rollback_buffer_bytes`
  now uses explicit `uint64_t` storage instead of `std::size_t`. The estimate
  still uses local `sizeof` as intended because it reports approximate memory
  usage, but the value copied through local state/debug views no longer carries
  host-width integer storage.
- Fixed in runtime animation state: `AFrameAnimator::current_frame` now uses
  explicit `uint32_t` storage instead of `std::size_t`. This field is entity and
  particle state, participates in fingerprints/snapshots, and was already
  serialized as `uint32_t`; the runtime representation now matches that
  contract. Display-state forced animation-frame selections now use the same
  explicit width.
- Fixed in local sprite presentation state: `SpriteAnimator::current_frame` now
  uses explicit `uint32_t` storage instead of `std::size_t`. This animator is
  not the entity authoritative animation state, but the index is bounded by
  authored sprite frame counts and no longer leaves a host-width counter in
  presentation/runtime animation state.
- Fixed in entity projectile-contact damage state: `Ent::proj_contact_damage_amount`,
  `EntSpec::proj_contact_damage_amount`, knockback projectile-contact damage,
  and shared damage callback/helper parameters now use explicit `uint32_t`
  instead of implementation-width `unsigned int`. Snapshot/replay bytes already
  stored the field as 32 bits; runtime storage and the gameplay callback API now
  match that contract directly. A money-pickup amount helper was tightened to
  the same explicit width because it feeds `Ent::money`, which is already
  `uint32_t`.
- Fixed in stage wrap runtime state: `Stage::wrap_padding_tiles` and the
  toroidal-wrap setup API now use explicit `uint32_t` instead of
  implementation-width `unsigned int`. Snapshot/replay bytes already stored the
  field as 32 bits, so runtime storage now matches the serialized stage
  contract.
- Fixed in unsigned vector runtime state: `UVec2` now stores explicit
  `uint32_t` components instead of implementation-width `unsigned int`.
  `UVec2` carries snapshot-visible tile, room, stage, input mouse, sprite, and
  settings dimensions, and the snapshot/replay format already writes those
  components as 32-bit integers. Local loop counters and stage indexing APIs
  still use native unsigned/index types where they are only indexing vectors.
- Fixed in gameplay constants: frame-rate, tile-size, player cooldown, stun/
  immunity/contact-duration, and damage constants that feed fixed-width
  simulation fields now use explicit `uint32_t` instead of
  implementation-width `unsigned int`. Local loop counters and vector indexes
  remain native index types where they do not become gameplay state.
- Fixed in tool spec gameplay metadata: preferred inventory slot ids now use
  explicit optional `uint32_t` storage instead of optional `std::size_t`, with
  casts kept at local two-slot array indexing boundaries.
- Fixed in bounded gameplay choice indexes: trap-block trigger direction
  selection and monkey stealable tool-slot candidate selection now store their
  chosen indexes as explicit `uint32_t`, with casts kept at fixed-array access
  boundaries.
- Fixed in synchronized settings state: control-binding ids now use explicit
  `uint32_t` runtime storage, and the settings parser plus snapshot/replay
  controls/debug-ui writers route through typed `uint32_t` helpers instead of
  implementation-width `unsigned int` helpers. The old unsigned recording
  helper path was removed.
- Fixed in shared snapshot/replay scalar helpers: `UVec2` reads now assign
  directly into explicit `uint32_t` components, and byte-shift helper loops use
  fixed-width counters instead of implementation-width `unsigned int`.
- Fixed in runtime fingerprint construction: byte-shift hashing helpers and
  stage-grid traversal now use explicit `uint32_t` counters at the fixed-width
  hash boundary instead of implementation-width `unsigned int`.
- Fixed in authored stage runtime state: `EntSpawn` link indices and
  `StageTileTrigger::target_spawn_index` now use explicit optional `uint32_t`
  storage instead of optional `std::size_t`. These are bounded authored spawn
  ids and were already encoded as optional `uint32_t` in snapshot/replay bytes;
  vector indexing now casts only at the local indexing boundary.
- Fixed in shared snapshot/replay format: stage scalar fields and contact
  cooldown expiry frames now use typed scalar helpers, removing raw
  `stage.*` / `entry.*` member POD calls from the recording writer.
- Fixed in shared snapshot/replay format: remaining small snapshot structs and
  ids now use typed scalar helpers, removing raw VID, stage-load, buyable,
  use-state, effect, spawn, stage-light, embedded-treasure, background-stamp,
  and tool-slot member POD calls from the recording writer.
- Fixed in shared snapshot/replay format: tile and tile-rotation grid elements
  now route through typed fixed-width helpers.
- Fixed in shared snapshot/replay format: local option/presence bytes, vector
  counts, grid dimensions, player/tool/topology collection counts, deterministic
  RNG state, net entity ids, and recording header fields now route through
  named fixed-width helpers. Raw POD use is now isolated to the scalar helper
  implementations in the recording writer.
- Fixed in shared snapshot/replay format: those scalar helper implementations
  now write/read explicit little-endian fixed-width integers and explicit
  `float` / `double` bit payloads. Recording format version is now 96.
- Fixed/guarded in byte-format scalar boundaries: the network packet encoder,
  runtime fingerprint writer, and debug recording scalar helpers now require
  IEC 559 / IEEE-754 `float` and `double` at compile time before encoding
  floating payload bits. This keeps the byte-format portability assumption
  explicit while the broader audit continues removing gameplay dependence on
  cross-platform float arithmetic.
- Fixed in desync replay diagnostics: SDRP reader/writer fields now use named
  fixed-width helpers for stage ids, frames, player ids, hash components, input
  counts, input records, and local entity hash diagnostics. This removes the
  local generic POD helper from SDRP metadata and keeps replay analysis files
  independent of accidental future type-width changes.
- Fixed in desync replay diagnostics: the SDRP `uint16_t` writer now masks with
  a width-explicit `uint16_t` constant before narrowing to bytes. This was
  caught by the ASan/UBSan strict build under `-Wsign-conversion`.
- Fixed in stage dimension/query APIs: `Stage::GetWidth`, `GetHeight`,
  `GetTileWidth`, `GetTileHeight`, and unsigned tile-grid accessor parameters
  now use explicit `std::uint32_t` instead of implementation-width
  `unsigned int`. Stage dimensions already store and serialize through
  `UVec2` / `uint32_t`; the runtime query API now matches that deterministic
  width, with native casts left only at local indexing or signed bounds-check
  boundaries.
- Audit checkpoint 2026-06-07: current targeted scan of deterministic-state
  headers and snapshot/fingerprint/network code found no remaining
  `std::optional<std::size_t>` runtime fields in gameplay or snapshot structs.
  The remaining `uintptr_t` / `long` uses are socket handles and platform socket
  calls inside `net_transport`, not synchronized gameplay state. Approximate
  network memory accounting still uses `sizeof`, but only as a diagnostic input
  to fixed-width telemetry.
- Audit checkpoint 2026-06-07: a focused scan of fingerprint, debug recording,
  snapshot, and network protocol writers found packet and snapshot byte formats
  using explicit-width integer, fixed-point, string-count, and IEEE-bit helper
  encoders at the serialization boundary. `std::size_t` still appears heavily
  as local vector/string indexing and buffer-size plumbing, but current
  fingerprint/network writers cast counts to explicit `uint32_t`/`uint64_t`
  encodings before hashing or writing bytes.
- Deferred risk: continue auditing snapshot/replay formats for any remaining
  platform-sized values before treating recordings as portable artifacts.

## Asset And Config Consistency Audit

- [x] Ensure gameplay-affecting config data is identical across peers.
- [x] Hash or version gameplay specs loaded from data files.
- [x] Ensure generated data and default profiles do not differ by platform.
- [x] Keep local user settings out of authoritative simulation unless explicitly
      synchronized.

### Status 2026-06-06

- Partially fixed. Gameplay-affecting runtime settings now round-trip through
  simulation snapshots, including fluid simulation, water movement, stage
  lighting, and player tuning settings.
- Audited current config container-order boundaries. Quest, glyph, item-pool,
  shop-type, room-pool, and pass-property `unordered_map` storage is currently
  used as keyed lookup/cache data in stage generation, not as randomized
  iteration order for gameplay choices. `LoadRoomTemplatesFromDirectory`
  canonicalizes `directory_iterator` results by sorting paths before loading.
- Audit checkpoint 2026-06-07: runtime quest exit ordering is canonicalized at
  the generation boundary. `QuestStageDefinition::exits` is stored as an
  `unordered_map`, but `GenerateClassicStage` copies the exit keys, sorts them,
  and then appends `StageExit` records in that stable order before authored
  `BasicExit` spawns resolve `stage_exit_id`. The remaining direct iteration of
  `stage.exits` in quest validation only affects diagnostic order, not
  gameplay state.
- Audit checkpoint 2026-06-07: the SID broadphase uses an `unordered_map` for
  cell buckets, but gameplay queries iterate deterministic tile-cell ranges and
  sort returned VIDs by `(id, version)` before callers process collisions or
  overlap results. Bucket hash iteration order is therefore not exposed to
  authoritative entity processing.
- Deferred risk: loaded quest/spec/profile files are not yet independently
  hashed or versioned as part of connection admission, so mismatched assets
  still rely on later state-fingerprint/desync detection rather than an early
  compatibility gate.
- Fixed for current classic quest content: direct UDP join requests now carry a
  deterministic hash of the loaded `assets/quests/classic` file tree. Hosts
  reject mismatched content before assigning players or entering the join
  barrier, and peers verify the host hash in the join accept before loading
  into lockstep. This covers current data-driven classic quest/stage/room/pool
  content, but not C++ gameplay code changes or future non-classic content
  roots.
- Deferred risk: this is an admission gate, not a full content-addressed asset
  manifest. Future quest roots, mod/plugin content, and authored data outside
  `assets/quests/classic` need to be added to the compatibility domain before
  they can safely affect authoritative gameplay.
- Audited current local profile/settings boundary. User profile and settings
  files remain local input/config sources; gameplay-affecting runtime settings
  are copied into synchronized state before they affect the simulation. Local
  profile defaults, input devices, and menu preferences must not be read
  directly from disk inside authoritative fixed-tick gameplay.

## Time/Input/UI Boundary Audit

- [ ] Ensure wall-clock time never directly affects gameplay simulation.
- [ ] Ensure render frame rate does not affect fixed simulation ticks.
- [ ] Ensure UI/menu state does not leak into simulation state except through
      explicit synchronized commands.
- [ ] Ensure local input capture is converted into deterministic input frames
      before simulation.

### Status 2026-06-06

- In progress. Gameplay advances through fixed simulation ticks, while wall-clock
  time remains in frame accumulation, rendering, UI, debug, and transport
  scheduling boundaries.
- Fixed in network validation timing: packet-fuzzer delay and bandwidth pacing
  now use local `RoundToInt` / `CeilToInt` instead of platform libm
  `std::lround` / `std::ceil`. This is not authoritative gameplay state, but it
  keeps simulated network conditions more reproducible across platforms.
- Fixed in lockstep delay selection: production and debug suggested input-delay
  calculations now use local `CeilToInt` instead of platform libm `std::ceil`.
  The suggested delay is network configuration, not direct gameplay state, but
  keeping it under the same rounding rules avoids cross-platform drift in
  validation and developer tuning.
- Fixed in CLI lockstep validation: the in-process packet fuzzer now uses local
  `CeilToInt` for delay tick conversion instead of platform libm `std::ceil`.
  This keeps stress/replay validation behavior aligned with runtime net fuzzer
  rounding.
- Audited mouse input boundary: mouse coordinates remain serialized in input
  records and included in full canonical/debug fingerprints, but the live
  network lockstep fingerprint uses `AddNetworkPlayerRegistryFingerprint` and
  does not hash input frames. Current non-debug gameplay reads only button
  inputs; mouse position reads are debug/playback/editor tooling. If mouse aim
  becomes authoritative gameplay later, it must be quantized intentionally and
  included in the network fingerprint with explicit semantics.
- Audit checkpoint 2026-06-07: the SDL wall-clock timers in `main.cpp` feed
  render/UI/debug pacing, performance stats, frame capping, and the fixed-tick
  accumulator. Authoritative gameplay ticks enter through `StepSingleTick` and
  call `StepPlaying` / `StepGameOver` with `kTimestep`, not arbitrary render
  frame delta. Debug playback also advances live simulation through
  `StepSingleTick` when stepping manually. The accumulator
  `time_since_last_update` is still serialized in snapshots as pacing state,
  but it is not included in the live network gameplay fingerprint and does not
  change per-tick simulation semantics.
- Deferred risk: UI/debug control paths still need a focused audit for any
  command that mutates authoritative gameplay state outside synchronized menu,
  input, or debug-only boundaries. The current scan did not find wall-clock
  values directly driving fixed-tick gameplay, but it does not close all UI
  command routing.
- Fixed in debug control command routing: the TCP debug `start-game` command
  now follows the same network start path as the Gubsy menu. Hosts schedule a
  synchronized `RequestRunStart`, peers reject local starts, and offline runs
  keep the old local `Mode::Playing` transition. The old debug command could
  flip only the local process into `Playing` during a hosted lobby, which was a
  dev-only but real path to unsynchronized gameplay state.
- Validation 2026-06-07: release build,
  `--check-join-barrier-next-stage-restart-smoke`,
  `--check-network-fresh-reload-ownership-smoke`, and
  `--check-state-equality-smoke` passed after the debug `start-game` routing
  fix.
- Validation 2026-06-07: `--check-input-lockstep-smoke` passed after the
  current time/input boundary audit checkpoint. The run covered clean,
  impaired, regional-latency, run-rate-skew, join-barrier, snapshot chunk,
  retained reconnect, carry transition, respawn policy, rollback repair,
  snapshot resync, hash exchange, stage-transition resync-block, hash rollback,
  and rollback-latency cases.

## Networking/Topology Determinism Audit

- [ ] Ensure join/leave/topology changes are applied at deterministic frame
      barriers.
- [ ] Ensure player-slot ordering is stable across host and peers.
- [ ] Ensure late inputs trigger rollback/resimulation consistently.
- [ ] Ensure resync snapshots restore every gameplay-affecting field.
- [ ] Keep Realnet/Gubsy transport metadata outside authoritative gameplay
      state unless explicitly synchronized.

### Status 2026-06-06

- In progress. The current lockstep path has join barriers, snapshot catchup,
  stable network entity ids, and sorted network fingerprints. Recent validation
  covered same-machine two-process UDP lockstep with confirmed hashes and no
  mismatches.
- Cleanup pass: join-barrier topology delta sets now canonicalize player ids
  for joined players, removed players, and topology-ack peers. The catchup
  queue intentionally remains FIFO because it is scheduling state, not
  authoritative player ordering.
- Validation 2026-06-07: release build passed, state equality and deterministic
  replay smokes passed, `--check-join-barrier-next-stage-restart-smoke` passed,
  and `--check-network-fresh-reload-ownership-smoke` passed after the
  platform-sized id cleanup. The broader `--check-input-lockstep-smoke` also
  passed under a 300 second cap, covering clean, impaired, same-house, same-city,
  same-state, cross-country, Japan/Texas, run-rate-skew, join barrier, retained
  reconnect, carry transition, respawn policy, rollback repair, snapshot resync,
  hash exchange, transition resync-block, and rollback-latency cases.
- Validation checkpoint 2026-06-07: after migrating join-accept spawn
  coordinates to Fixed12 packet fields and removing raw float packet helpers,
  the broader `--check-input-lockstep-smoke` passed again under a 360 second
  cap. Earlier short timeouts were validation-harness impatience, not a
  lockstep hang; the aggregate smoke is simply expensive because it runs many
  1200-frame two-peer latency profiles plus topology/rollback cases.
- Deferred risk: topology changes during active play need broader validation
  with multiple local players per peer, high latency, reconnect, stage
  transition, restart run, and relay/NAT paths.

## Final Audit Result

- [ ] Record every remaining authoritative nondeterminism risk.
- [ ] Mark each remaining risk as fixed, intentionally quarantined, or accepted
      with a reason.
- [ ] Validate with local two-client play.
- [ ] Validate with cross-machine play when available.
- [ ] Preserve enough SDRP/desync data to debug any new divergence found during
      validation.
