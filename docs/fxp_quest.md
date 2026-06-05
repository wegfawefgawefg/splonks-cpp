# FXP Quest

## Why This Exists

Splonks multiplayer is now using deterministic lockstep with rollback/resync. The
game sends inputs and topology updates, then expects every machine to simulate
the same world. That makes simulation determinism a gameplay requirement, not a
nice-to-have.

During real cross-machine playtests, a Linux/x86_64 host and macOS/arm64 peer
could produce desyncs even when the input stream and room state looked correct.
The current replay/desync capture work showed one important case:

- Kyle's Mac was the peer.
- The matching Linux host desync replay for that session was not available.
- The peer replay reproduced the host/captured-remote hash on Linux.
- Kyle's captured local hash differed in `ents`.
- The first visible differing entity was a `GoldStack`.
- `GoldStack` uses ordinary entity physics/collision rather than special
  networking or lobby behavior.

That points at cross-machine simulation drift in gameplay state, not at Gubsy
room discovery, Realnet NAT/relay negotiation, player topology, or input packet
ordering.

Splonks currently hashes exact float bits for important networked entity fields
such as position, velocity, acceleration, size, and rotation. Gameplay physics
also uses floats for integration, friction, gravity, collision movement, and
branch decisions. That combination is fragile in deterministic lockstep: tiny
floating-point differences can accumulate, cross branch thresholds, then become
real state divergence.

This quest exists to investigate and build a fixed-point path for deterministic
gameplay math.

## What We Learned

- This is not about rendering. Rendering, camera smoothing, UI, audio, particles,
  and visual-only interpolation can stay float.
- This is about gameplay state that affects simulation, branches, collision,
  entity spawning, damage, pickups, hazards, and lockstep hashes.
- Most authored gameplay decimal constants in source are low precision:
  one or two decimal places dominate, three decimal places appear, and only a
  small number of gameplay-ish constants go beyond that.
- A binary fixed-point scale such as `1 pixel = 4096 subpixels` gives about
  `0.00024` pixel precision, which is enough for the values we normally author
  and gives velocity/acceleration more room than the first `1/1024 px` pass.
- The main cost of fixed-point is migration and API discipline, not raw CPU time.
- We still need actual benchmarks before committing Splonks physics to it.

## Initial Direction

Create a small fixed-point companion library named `gfxp`.

Use the existing G&K/Gubsy companion library style as reference:

- `ginput`
- `glayout`
- `gmenu`
- `gcore`
- `gaudio`
- `gnetcode`
- `gparticles`
- `ganim`

The `glayout` repo should be cloned locally and used as the formatting/build
style reference:

```sh
cd /home/vega/Coding/GameDev
git clone https://github.com/wegfawefgawefg/glayout
```

The `gfxp` library is for investigation and reusable development first. Once it
is proven, Splonks can vendor/copy the necessary code directly. We should not
make Splonks permanently depend on a sibling checkout just to ship the game.

## Goals

- Provide a tiny, deterministic fixed-point number type for game simulation.
- Keep the API small enough to audit.
- Make hashing and serialization obvious.
- Make rounding behavior explicit.
- Preserve enough precision for Splonks authored movement/physics values.
- Benchmark against float operations and a representative physics loop.
- Keep distribution simple by allowing the proven implementation to be copied
  into Splonks or Gubsy later.

## Non-Goals

- Do not replace render/camera/UI/audio float math.
- Do not create a broad numeric framework.
- Do not integrate into Splonks physics before the isolated library is tested.
- Do not add a permanent sibling-repo dependency to Splonks for release builds.
- Do not optimize prematurely around synthetic microbenchmarks only.

## Proposed API Shape

Start minimal:

```cpp
namespace gfxp {

struct Fixed {
    int32_t raw = 0;

    static constexpr int frac_bits = 12;
    static constexpr int scale = 1 << frac_bits;
};

struct Vec2 {
    Fixed x;
    Fixed y;
};

} // namespace gfxp
```

Likely operations:

- construction from integer pixels
- deterministic construction from decimal strings
- explicit construction from float for debug/render interop only
- addition/subtraction
- multiplication with `int64_t` intermediates
- division with explicit rounding
- comparison
- floor/ceil/round/trunc to integer pixels
- clamp/min/max/abs/sign
- serialization to raw integer
- stable hash support
- vector add/sub/scale

The first candidate scale is:

```cpp
1 pixel = 4096 subpixels
```

That is binary fixed point with 12 fractional bits. It is precise enough for
roughly four decimal places while still allowing cheap shifts and enough world
range with `int32_t` raw storage. Multiplication should use `int64_t`
intermediates.

## Header-Only Question

We should consider whether `gfxp` should be header-only.

Reasons header-only may be good:

- Easy to vendor/copy into Splonks.
- Most operators are tiny and inline-friendly.
- No link/distribution burden.
- Future games can copy one include tree.

Reasons to avoid committing to header-only immediately:

- Benchmarks, tests, and parsing helpers may be cleaner with `.cpp` files.
- We may want a non-header implementation while experimenting.
- A generated single-header release can be made later if useful.

Plan: write normal clean library code first. If the core becomes naturally
header-only, keep it that way. If not, make a vendorable header-only package
or copied source package after the API settles.

## Benchmark Plan

Benchmarks must include both primitive operations and Splonks-like loops.

Primitive comparisons:

- float add/sub/mul/div
- fixed add/sub/mul/div
- double add/sub/mul/div
- optional comparison against `fpm`

Representative loops:

- `vel += acc`
- friction/drag style damping
- `pos += vel`
- floor/trunc to pixel
- per-axis movement step
- simple tile collision branch
- stable hashing of state

Benchmarks should report:

- throughput
- per-entity update cost
- compiler flags
- platform/CPU
- integer scale used
- whether the test is branch-heavy or arithmetic-heavy

Target platforms to test eventually:

- Linux x86_64 desktop
- macOS arm64
- Windows x86_64

## Determinism Tests

The first tests should prove exact behavior, not just speed.

- Arithmetic identities that should hold exactly.
- Rounding mode tests for positive and negative values.
- Decimal parsing tests for common authored constants.
- Serialization round-trip tests.
- Hash stability tests.
- Movement integration test with fixed input over thousands of ticks.
- Collision stepping test with thresholds near tile boundaries.

## Repository Setup Task List

- [x] Clone `glayout` into `/home/vega/Coding/GameDev`.
- [x] Inspect `glayout` build scripts, formatting rules, README style, license,
      and repository layout.
- [x] Create sibling repo `/home/vega/Coding/GameDev/gfxp`.
- [x] Set up the same compile style and formatting rules as `glayout`.
- [x] Add a small, dry README matching the tone of the other `g*` libraries.
- [x] Add a similar simple SVG/logo.
- [x] Add initial docs/spec describing the library goals, non-goals, scale,
      rounding rules, serialization, hashing, and Splonks usage path.
- [x] Add initial fixed-point type and vector type.
- [x] Add deterministic tests.
- [x] Add benchmark executable(s).
- [x] Run local build/tests/benchmarks.
- [x] Create the GitHub repo with `gh`.
- [x] Push the initial repository to GitHub.

## Initial gfxp Result

Created and pushed:

```text
https://github.com/wegfawefgawefg/gfxp
```

Local checkout:

```text
/home/vega/Coding/GameDev/gfxp
```

Initial commit:

```text
1fef62b Initial gfxp fixed-point library
```

Validation:

```sh
./scripts/build.sh
./scripts/bench.sh
```

Linux release benchmark from the first pass:

```text
entities: 20000, ticks: 2000
fixed scale: 1024 subpixels/pixel
float: 0.0349619s, 0.874048 ns/entity-tick
double: 0.0318365s, 0.795913 ns/entity-tick
fixed: 0.0447168s, 1.11792 ns/entity-tick

primitive ops: 67108864
float add: 0.34168 ns/op
double add: 0.345594 ns/op
fixed add: 0.0467201 ns/op
float mul: 0.508866 ns/op
double mul: 0.508267 ns/op
fixed mul: 0.874222 ns/op
float div: 1.72492 ns/op
double div: 2.23933 ns/op
fixed div: 1.30987 ns/op
```

Interpretation:

- Fixed add is extremely cheap.
- Fixed multiply is slower than float/double multiply in this benchmark because
  it uses an `int64_t` intermediate and scale divide.
- Fixed division is faster than float/double division in this benchmark.
- The representative movement loop is close enough to justify deeper testing.
  Collision/map queries are likely to dominate more than scalar arithmetic in
  Splonks.

## Current gfxp Direction

The second `gfxp` pass changed the default to `Fixed12`:

```cpp
using gfxp::Fixed; // int32_t raw, 12 fractional bits, 1/4096 px
```

Scale-specific aliases now exist for comparison:

```cpp
gfxp::Fixed8;
gfxp::Fixed10;
gfxp::Fixed12;
gfxp::Fixed16;
```

Latest pushed `gfxp` commit:

```text
ef0116e Use 12-bit fixed scale and benchmark variants
```

The fixed multiply path now uses a shift instead of division:

```cpp
raw = (int64_t(lhs.raw) * rhs.raw) >> frac_bits;
```

The optimized x86_64 release assembly for `Fixed12` is:

- Add: single `lea`.
- Multiply: sign-extend inputs, `imul`, `sar $0xc`.
- Divide: `shl $0xc`, `idiv`.

Linux release benchmark from that pass:

```text
entities: 20000, ticks: 2000
default fixed scale: 4096 subpixels/pixel
float:   0.890038 ns/entity-tick
double:  0.799296 ns/entity-tick
fixed8:  0.630493 ns/entity-tick
fixed10: 0.985434 ns/entity-tick
fixed12: 0.984826 ns/entity-tick
fixed16: 0.932477 ns/entity-tick

primitive ops: 67108864
float add:  0.339631 ns/op
double add: 0.339721 ns/op
fixed add:  0.0520846 ns/op
float mul:  0.511959 ns/op
double mul: 0.513509 ns/op
fixed mul:  0.524354 ns/op
float div:  1.75691 ns/op
double div: 2.35218 ns/op
fixed div:  1.32789 ns/op
```

Current conclusion: `Fixed12` is a good default candidate for Splonks
experiments. It has much more precision than position strictly needs, enough
range for huge Splonks stages, and fixed multiply is now essentially tied with
float multiply in the primitive benchmark.

## gfxp Ergonomics Plan

Before Splonks integration, `gfxp` needs a small API ergonomics pass so using it
does not become tedious or error-prone.

- [ ] Add a templated `BasicVec2<FixedT>` so experiments can use `Fixed8`,
      `Fixed10`, `Fixed12`, or `Fixed16` vectors without rewriting vector code.
- [ ] Keep `Vec2` as the default `BasicVec2<Fixed>`.
- [ ] Add explicit fixed-to-fixed conversion helpers, such as
      `fixed_cast<ToFixed>(value)`, with named rounding.
- [ ] Add named pixel helpers, such as `from_pixels`, `checked_from_pixels`,
      `to_pixels_floor`, `to_pixels_ceil`, and `to_pixels_round`, while keeping
      raw integer access for hashes.
- [ ] Add checked and unchecked hot-path helpers where the distinction matters.
      Debug builds should make overflow policy easy to audit.
- [ ] Add deterministic vector helpers needed by physics migration:
      `dot`, `length_sq`, `manhattan_length`, component min/max/clamp, and
      sign helpers.
- [ ] Do not add `sqrt`, `sin`, `cos`, `atan2`, or normalize casually. Gameplay
      trig/length needs a separate deterministic design.
- [ ] Keep render-boundary float conversion explicit.
- [ ] Keep the core header-only unless a real reason appears not to.

## Splonks Determinism Audit Plan

The broader post-FXP determinism audit now lives in
`docs/plans/determinism_audit_plan.md`.

Fixed-point math reduces one large source of desyncs, but it is not a full
determinism guarantee by itself. The dedicated audit plan tracks the remaining
risks: gameplay floats, math functions, iteration order, RNG, serialization,
undefined behavior, platform-sized types, data consistency, time/input
boundaries, and networking topology barriers.

## Splonks Integration Task List

This happens only after the isolated library is proven.

- [x] Decide whether Splonks vendors `gfxp` directly or receives a copied subset.
      Decision: vendor the small header-only include tree directly into
      Splonks. Do not make Splonks depend on a sibling `../gfxp` checkout.
- [ ] Decide whether Gubsy should also vendor it for future game/tooling use.
- [x] Identify the current Splonks float inputs inside fingerprint/hash domains.
- [ ] Keep render conversion explicit at the render boundary.
- [x] Keep network hashes over raw fixed-point integers.
- [ ] Update SDRP/desync captures to include enough final local state to compare
      exact entity field differences.
- [x] Remove raw float bit hashing from authoritative hash domains by migrating
      values to fixed-point/integer state or hashing an explicit fixed-point
      quantized representation.
- [ ] Validate Linux host/macOS peer determinism with the same recorded input
      session.

## Splonks First Integration Scope

The next Splonks step should not stop at vendoring. It should vendor `gfxp`,
define the local fixed-point vocabulary, and start removing raw floats from the
lockstep/hash domain.

The goal is not "one narrow movement path." That was too small. The real target
is every gameplay-authoritative float that currently contributes to canonical,
gameplay determinism, or network fingerprints. Implementation can still be
phased by category, but the scope is the full hashed domain.

Rule: any gameplay-authoritative value included in lockstep/network/canonical
hashes must be fixed-point, integer/discrete, or intentionally quantized before
hashing. Raw float bit hashing is the thing this project is eliminating. Pure
render, camera, UI, audio, and cosmetic values can stay float if they do not
feed authoritative hashes.

### Vendor gfxp

- [x] Copy the upstream `gfxp/include/gfxp` header tree into Splonks.
- [ ] Put the vendored copy under an explicit vendor path:

  ```text
  src/vendor/gfxp/include/gfxp/
  ```

- [x] Add a vendoring note:

  ```text
  src/vendor/gfxp/README.md
  upstream: https://github.com/wegfawefgawefg/gfxp
  vendored commit: fb10177
  update method: copy include/gfxp from upstream
  ```

- [x] Add the vendor include path to Splonks CMake.
- [ ] Do not introduce a runtime or build dependency on `../gfxp`.
- [ ] Do not vendor `gfxp` into Gubsy in this step.

### Define Splonks Fixed-Point Vocabulary

Add a thin Splonks-owned header that gives gameplay code domain names instead of
using upstream aliases everywhere:

```text
src/sim/fxp.hpp
```

Proposed contents:

```cpp
namespace splonks::sim {
using Scalar = gfxp::Fixed12;
using Vec2 = gfxp::BasicVec2<Scalar>;
}
```

This keeps upstream `gfxp` generic while giving Splonks room to later choose a
different scalar or add game-specific helpers without editing the vendored copy.

Tasks:

- [x] Add `src/sim/fxp.hpp`.
- [x] Define `splonks::sim::Scalar` and `splonks::sim::Vec2`.
- [x] Add conversion helpers between current `splonks::Vec2` and
      `splonks::sim::Vec2`.
- [x] Keep conversion names explicit, such as `ToSimVec2` and `ToRenderVec2`,
      so render-boundary float conversion is visible.

### Hash Domain Float Audit

Original audit source: `src/state_fingerprint.cpp`.

Before the first implementation pass, `FingerprintWriter::AddFloat(float)`
copied the raw IEEE float bits into the hash. `FingerprintWriter::AddVec2(const
Vec2&)` called `AddFloat` for `x` and `y`. These were the float inputs that had
to be removed from raw bit hashing:

Stage fingerprints:

- `stage.gravity`
- `stage.fluid_amount[y][x]`

Entity effect fingerprints:

- `effect.value`

Canonical/gameplay entity fingerprints:

- `ent.pos.x`, `ent.pos.y`
- `ent.vel.x`, `ent.vel.y`
- `ent.acc.x`, `ent.acc.y`
- `ent.size.x`, `ent.size.y`
- `ent.rotation`
- `ent.counter_a`
- `ent.counter_b`
- `ent.counter_c`
- `ent.counter_d`
- `ent.light_strength`
- `ent.light_color.r`
- `ent.light_color.g`
- `ent.light_color.b`
- `ent.aframe_animator.current_time`
- `ent.aframe_animator.speed`
- every `effect.value` on `ent.effects`

Network entity fingerprints:

- For normal entities:
  - `ent.pos.x`, `ent.pos.y`
  - `ent.vel.x`, `ent.vel.y`
  - `ent.acc.x`, `ent.acc.y`
  - `ent.size.x`, `ent.size.y`
  - `ent.rotation`
  - `ent.counter_a`
  - `ent.counter_b`
  - `ent.counter_c`
  - `ent.counter_d`
  - every `effect.value` on `ent.effects`
- For entities whose motion is ignored for player ownership/held-object sync:
  - motion, size, rotation, and counters are skipped
  - `effect.value` is still hashed

Non-float but related audit note:

- `FingerprintWriter::AddString` hashes `std::string::size()` directly. That is
  not the float determinism problem, but a later cross-platform hash cleanup
  should replace platform-sized fields with explicit-width values.

### Migration Classification

P0: authoritative gameplay values that should become fixed-point state or
fixed-point hash representations first:

- Entity motion and shape: `pos`, `vel`, `acc`, `size`, and gameplay-affecting
  `rotation`.
- World physics fields: `stage.gravity`.
- Fluid amount if fluid affects movement, collision, damage, or spawning.
- `effect.value` when the effect changes gameplay.
- `counter_a` through `counter_d` where they drive AI, timers, movement,
  damage, spawning, or contact behavior.

P1: values that may be better converted to integer/tick state rather than fixed
point:

- `aframe_animator.current_time` and `aframe_animator.speed` if animation state
  affects gameplay, hitboxes, contact, or spawn timing.
- Generic counters whose actual meaning is a frame count, cooldown, state age,
  or phase index.

P2: visual-only values that should not participate in network lockstep hashes:

- `light_strength`
- `light_color.r`, `light_color.g`, `light_color.b`

These are already absent from `AddNetworkEntFingerprint`, but they remain in
canonical/gameplay hashes. Either keep them only in debug/canonical hashes with
explicit quantization or remove them from gameplay determinism hashes if they
are truly cosmetic.

### Hash-Domain Migration Strategy

The first implementation pass should cover the hash boundary and the highest
risk gameplay fields. It can bridge from the existing float storage where needed,
but the bridge must be explicit and temporary.

Tasks:

- [x] Add helpers for quantizing float `Vec2` and scalar floats to `sim::Vec2`
      / `sim::Scalar`.
- [x] Replace raw `AddFloat` hashing for authoritative fields with fixed raw
      integer hashing.
- [ ] Prefer migrating storage to fixed-point for P0 gameplay fields over
      leaving long-term float storage with hash-only quantization.
- [ ] Convert generic movement/physics stepping for `pos`, `vel`, and `acc` to
      fixed-point state or fixed-point intermediates.
- [x] Audit each use of `counter_a` through `counter_d` and classify it as
      fixed-point, integer tick/state, or non-authoritative.
- [x] Audit `effect.value` definitions and classify each effect as gameplay or
      cosmetic.
- [x] Audit `stage.fluid_amount` and decide whether it is gameplay or cosmetic.
- [x] Audit `aframe_animator` usage and decide whether animation time is
      gameplay state or render-only state.
- [ ] Keep render and non-authoritative visual code float.
- [ ] Build and run local play to confirm no obvious feel breakage.
- [ ] Re-run a two-machine Linux/macOS or Linux/Linux multiplayer check before
      expanding the migration.

### First Implementation Notes

Current implementation status:

- `src/vendor/gfxp/include/gfxp` contains the vendored header-only library from
  upstream commit `fb10177`.
- `src/sim/fxp.hpp` defines `splonks::sim::Scalar` as `gfxp::Fixed12`,
  `splonks::sim::Vec2`, `ToSimScalar`, `ToSimVec2`, and `ToRenderVec2`.
- `src/state_fingerprint.cpp` no longer hashes raw IEEE float bits. All
  previously audited float inputs now hash the raw `Fixed12` value produced by
  the explicit Splonks conversion boundary.

Classification result:

- `counter_a` through `counter_d`: gameplay-authoritative. Most current uses
  are frame counters, state enums/flags stored as floats, ammo/value buckets, or
  cooldowns. Some uses are distance/phase accumulators, such as trail intervals,
  moving-platform phase, and debug moving lights. Short term: quantized hash.
  Longer term: migrate each counter use to typed integer state or fixed-point
  fields based on meaning.
- `effect.value`: gameplay-authoritative when effects modify stats, tuning, or
  player state. Current effect specs include scalar modifier values, so it stays
  in the authoritative hash through fixed-point quantization. Pure render effect
  values should eventually move out of authoritative effect state.
- `stage.fluid_amount`: gameplay-authoritative stage state. It is simulated in
  `stage_fluids.cpp`, queried from stage data, rotated/wrapped with stage
  geometry, and should remain in stage/network fingerprints. Short term:
  quantized hash. Longer term: migrate stage fluid amount storage to fixed-point
  or a compact integer unit.
- `aframe_animator.current_time` and `aframe_animator.speed`:
  gameplay-authoritative for ents whose animation frame gates actions, contact,
  strikes, or state transitions. Short term: quantized hash. Longer term: use
  integer frame/subframe counters for gameplay animations and leave purely
  visual sprite timing outside gameplay hashes.
- `light_strength` and `light_color`: cosmetic for network lockstep and already
  absent from network entity hashes. They remain quantized in canonical/debug
  hashes until canonical hash modes are split more explicitly.

Validation so far:

- [x] `./scripts/build.sh`
- [x] `./build/splonks-cpp --check-state-fingerprint-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-join-barrier-next-stage-restart-smoke --project-root ...`
- [x] `python3 scripts/validate_lockstep_live.py --launch-pair --profile same-house ...`
      passed with confirmed hash frame `330`, zero hash mismatches, and no
      reported problems.
- [ ] `./build/splonks-cpp --check-input-lockstep-smoke --project-root ...`
      was stopped after running silently for about two minutes.

### What Not To Migrate First

Do not start with these unless they block the hashed authoritative domain:

- [ ] Particle simulation.
- [ ] Camera/group camera.
- [ ] UI/menu layout.
- [ ] Audio emitters.
- [ ] Lighting visuals, except for deciding whether they belong in
      canonical/gameplay hashes.
- [ ] Stage generation internals, except for seed and generated authoritative
      state that already lands in hashes.
- [ ] Trig-heavy gameplay.
- [ ] Non-hashed render-only entity fields.
- [ ] SDRP format changes beyond any hash visibility needed for this step.

### Success Condition

- Splonks contains a traceable vendored `gfxp` copy.
- Splonks has a local `sim::Scalar` / `sim::Vec2` vocabulary.
- Every current raw float hash input is either migrated to fixed-point/integer
  state, hashed through an explicit fixed-point quantization, or documented as
  non-authoritative and removed from the relevant gameplay/network hash.
- No gameplay-authoritative network/lockstep hash depends on raw float bits.
- Normal Splonks build passes.
- The game still feels plausibly the same in a quick local run.

## Open Questions

- Is `1/4096 px` the final Splonks scale, or should specific domains use
  smaller fixed formats such as `Fixed8` for compact storage?
- Should decimal constants be parsed from strings for exact authoring behavior,
  or is controlled compile-time conversion enough for hardcoded constants?
- Should `Fixed` allow implicit construction from integers, or should all
  conversions be explicit?
- Should division support multiple named rounding modes?
- Should overflow checks be always-on in debug builds?
- Should the public namespace be `gfxp` or a longer `gk::fxp` style namespace?
