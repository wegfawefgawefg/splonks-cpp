# Current Cleanup Scratch

Short-term triage list for the current large commit.

1. [x] Move authored tile CBox lookup out of common contact damage.
   - User: probably yes.
   - Done: common contact damage now reads spike CBoxes through `TileContactData`, not renderer-owned `TileSourceData`.
2. [x] Stomp flag audit for enemies, props, carried/thrown items.
   - User: yes, but many weird cases will be found by playtest.
   - Fixed: generated chests/key chests spawn at the tile top-left instead of halfway embedded.
   - Fixed: thrown chests/key chests now apply projectile contact damage like hard props.
   - Accepted: broad stomp-flag oddities will continue through normal playtest instead of blocking this pass.
3. [~] Loot/shop/item pools audit.
   - User: yes.
   - Fixed: crate contents now match ClassicHD's actual open-crate sequential roll order, except `Shotgun` intentionally maps to `Pistol`.
   - Fixed: crate fallback matches the HD/2 common tail: `1/2` `RopePile`, otherwise guaranteed `BombBag`.
   - Fixed: item pools now support per-entry `unique: true` so singleton gear such as `Compass` cannot duplicate in shops, while supply pools can still repeat bombs/ropes.
   - Fixed: `Telepack` now appears anywhere Classic quest data can roll `Teleporter`, with matching weights.
   - Remaining: shop/category pools are still weighted approximations unless we port ClassicHD's sequential roll logic.
4. [ ] Manual Mines 1 playtest pass.
5. [ ] Later Classic stage parity, starting with Jungle.
6. [ ] Remaining unimplemented Classic entity stubs.
7. [ ] Final water and lava gameplay behavior.
   - Planning note added to `docs/water_fluid_simulation.md`.
   - Current code already refreshes `EffectId::InWater` from water/fluid overlap, but `InWater` has no gameplay modifiers yet.
8. [ ] Expand transient light coverage.
9. [x] Profile dense live-light scenes.
   - Added `LightingStressTest` debug preset with adjustable moving colored entity-light count.
   - Playtest result: acceptable performance with 1024 moving light entities; render stayed around `1ms`, present around `5ms`.
10. [ ] Remove/update temporary fluid validation defaults and decide future optimization path.
11. [ ] Refresh passive/effect modifier plan against current implementation.
12. [ ] Retire or update stale `splk_mines_olmec_demo_checklist.md`.
# Stagegen Validation Follow-Up

- [ ] Make `--check-classic-quest-stagegen` deterministic/debuggable.
  - Observed one transient `std::bad_alloc` during randomized classic stagegen
    validation after `classic_temple_1`; immediate rerun passed.
  - The check currently does not print or pin per-stage RNG seeds, so failures
    are hard to reproduce.
  - Add per-stage seed reporting and failure context before investigating
    further.
