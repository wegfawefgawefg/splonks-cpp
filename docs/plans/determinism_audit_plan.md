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

## Math Function Audit

- [ ] Find gameplay uses of `std::sin`, `std::cos`, `std::tan`, `std::atan2`,
      `std::sqrt`, `std::hypot`, `std::pow`, `std::fmod`, `std::round`,
      `std::floor`, and `std::ceil`.
- [ ] Decide which uses can remain render-only.
- [ ] Replace gameplay-affecting trig with deterministic tables, discrete
      direction vectors, or fixed/integer approximations.
- [ ] Replace gameplay-affecting length/normalize code with deterministic
      alternatives or avoid normalization in authoritative state.

## Container And Iteration Order Audit

- [ ] Search deterministic simulation code for unordered containers.
- [ ] Ensure entity iteration order is stable.
- [ ] Ensure contact/collision pair ordering is stable when multiple candidates
      tie.
- [ ] Ensure maps keyed by pointers, addresses, or allocation order do not affect
      gameplay results.
- [ ] Avoid sorting by non-stable values or platform-dependent comparisons.

## RNG Audit

- [ ] Identify every RNG stream.
- [ ] Separate deterministic gameplay RNG from visual/debug/UI randomness.
- [ ] Ensure all gameplay RNG is seeded from synchronized state.
- [ ] Ensure joining peers receive exact RNG state in snapshots.
- [ ] Remove or quarantine process-global RNG use from gameplay.

## Serialization And Snapshot Audit

- [ ] Ensure deterministic snapshots use explicit integer sizes and endian rules.
- [ ] Avoid serializing `size_t`, `long`, pointers, enum storage assumptions, or
      padding bytes.
- [ ] Ensure all gameplay fields that affect simulation are included in
      snapshots/resync state.
- [ ] Ensure desync replay files include enough final local state to diff entity
      fields, not just hashes.

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

## Final Audit Result

- [ ] Record every remaining authoritative nondeterminism risk.
- [ ] Mark each remaining risk as fixed, intentionally quarantined, or accepted
      with a reason.
- [ ] Validate with local two-client play.
- [ ] Validate with cross-machine play when available.
- [ ] Preserve enough SDRP/desync data to debug any new divergence found during
      validation.
