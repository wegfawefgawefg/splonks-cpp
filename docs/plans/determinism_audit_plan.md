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
  `acc`, `size`, `rotation`, `counter_a` through `counter_d`,
  `AFrameAnimator::current_time`, `AFrameAnimator::speed`,
  `Stage::gravity`, and `Stage::fluid_amount` are the highest-priority
  simulation fields because they affect movement, contact, animation gates,
  world state, or lockstep fingerprints.
- Deferred risk: these fields are still simulated as float. The current
  quantized hash can prevent false cross-ISA mismatches from tiny float-bit
  differences, but it does not prevent two peers from crossing different branch
  thresholds before quantization. The follow-up migration is to move
  authoritative gameplay storage/math to fixed-point, integer counters, or
  explicit threshold quantization.

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
- Deferred risk: circular moving platforms currently use `std::sin` /
  `std::cos` in gameplay movement. Ball-and-chain uses `std::sqrt` for
  authoritative pull direction. These should be converted to deterministic
  lookup/fixed-point math or discrete approximations.
- Fixed for circular moving platforms: runtime `std::sin` / `std::cos` were
  replaced with a fixed 80-step integer unit-circle table. The platform still
  writes float `pos` / `vel` for the current physics pipeline, but its path
  selection and pixel offset generation no longer depend on platform libm.
- Fixed for ball-and-chain: runtime `std::sqrt` was replaced with fixed-scale
  integer length/direction math for catchup, taut-chain pull, and player
  pullback. The entity still reads/writes float `pos`, `vel`, and `acc` at the
  current physics boundary, but the pull direction no longer depends on
  platform libm.
- Deferred risk: shared world/tile query boundaries still use `std::floor` /
  `std::fmod` over float positions and need a fixed-point boundary pass.
- Fixed in network fingerprints: `Ent::rotation` is treated as cosmetic/render
  state and no longer participates in the network lockstep hash. This removes
  render-only `std::atan2` / `std::fmod` rotation drift from desync detection
  while preserving rotation in canonical/debug snapshots and rendering.

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
- Deferred risk: contact/collision tie ordering is not yet fully audited.

## RNG Audit

- [ ] Identify every RNG stream.
- [ ] Separate deterministic gameplay RNG from visual/debug/UI randomness.
- [ ] Ensure all gameplay RNG is seeded from synchronized state.
- [ ] Ensure joining peers receive exact RNG state in snapshots.
- [ ] Remove or quarantine process-global RNG use from gameplay.

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
- Deferred risk: audit every remaining `rng::` call and either prove it is
  non-authoritative or move it to `state.drng`.

## Serialization And Snapshot Audit

- [ ] Ensure deterministic snapshots use explicit integer sizes and endian rules.
- [ ] Avoid serializing `size_t`, `long`, pointers, enum storage assumptions, or
      padding bytes.
- [ ] Ensure all gameplay fields that affect simulation are included in
      snapshots/resync state.
- [ ] Ensure desync replay files include enough final local state to diff entity
      fields, not just hashes.

### Status 2026-06-06

- In progress. Snapshot vector and string counts are generally serialized with
  explicit `uint32_t` counts.
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
- Deferred risk: `SerializeSimSnapshotToBytes` / `DeserializeSimSnapshotFromBytes`
  still write many trivially-copyable structs by raw host layout. That means
  endian, enum storage, bool representation, float bit representation, and
  padding are not a finished cross-platform network format. The transport
  format should still be replaced with explicit field writers before depending
  on heterogeneous Windows/macOS/Linux peers.

## Undefined And Uninitialized Behavior Audit

- [ ] Run sanitizers locally where practical.
- [ ] Audit constructors/default initialization for gameplay structs.
- [ ] Avoid reading padding/uninitialized bytes in hashes.
- [ ] Avoid signed overflow in deterministic code.
- [ ] Avoid shift undefined behavior.
- [ ] Avoid aliasing assumptions that can differ by compiler.

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
- Deferred risk: continue auditing snapshot/replay formats for any remaining
  platform-sized values before treating recordings as portable artifacts.

## Asset And Config Consistency Audit

- [ ] Ensure gameplay-affecting config data is identical across peers.
- [ ] Hash or version gameplay specs loaded from data files.
- [ ] Ensure generated data and default profiles do not differ by platform.
- [ ] Keep local user settings out of authoritative simulation unless explicitly
      synchronized.

## Time/Input/UI Boundary Audit

- [ ] Ensure wall-clock time never directly affects gameplay simulation.
- [ ] Ensure render frame rate does not affect fixed simulation ticks.
- [ ] Ensure UI/menu state does not leak into simulation state except through
      explicit synchronized commands.
- [ ] Ensure local input capture is converted into deterministic input frames
      before simulation.

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
