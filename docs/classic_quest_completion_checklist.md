# Classic Quest Completion Checklist

This tracks whether each Classic Quest stage is complete enough to call
"correct" for the engine demo. A stage is not done just because it loads.

## Completion Criteria

Check a stage only when all applicable items are true:

- [ ] Room pools exist, load without fallback, and cover every room code the stage can request.
- [ ] Glyph mapping is complete for the source room data and never leaves accidental voids.
- [ ] Ambient entity pass matches the intended Classic/HD enemy/item mix for that area.
- [ ] Special layout events and branch exits are implemented with correct odds and route rules.
- [ ] Area hazards, fluids, traps, treasure, shops, altars, entrances, and exits behave correctly.
- [ ] Required entities have real behavior, not only `NoSprite` unimplemented archetype stubs.
- [ ] Debug reroll/check command can generate the stage repeatedly without hard errors.
- [ ] Manual playtest finds a valid entrance-to-exit route and no obvious generation blockers.

## Main Route

- [ ] `classic_mines_1` / `1-1` / Mines
  - Data loads and current gameplay is furthest along.
  - Needs final parity audit for enemy/object odds, pots/crates, snake pit odds, shop odds, altar odds, idol/tiki path, and treasure placement.

### `classic_mines_1` Sub Checklist

- [x] Room pools load and cover every possible Mines 1 room request.
  - Current pools exist for `start`, `main`, `side`, `drop`, `exit`, `exit_main`, `shop_left`, `shop_right`, `snake_pit_top`, and `snake_pit_bottom`.
  - Mines does not enable any layout pass that can request `special_6`, `special_7`, `special_8`, or `special_9`.

- [x] Full stagegen check can run without unrelated later-stage hard errors.
  - `build/splonks-cpp --check-classic-quest-stagegen` now generates every configured Classic stage through `classic_olmec_lair`.
  - Room selector now hard-validates ClassicHD pool counts per stage instead of accepting generic fallback shapes.
  - Removed non-exit data from the Black Market exit pool and corrected Ice Caves drop rooms to use the ClassicHD main-room pool.

- [x] Glyph mapping is audited against the source Mines room data.
  - Static glyph coverage is enforced by `build/splonks-cpp --check-classic-quest-stagegen`; every configured room pool is scanned, not only sampled generated rooms.
  - Mines room glyphs are covered: `+`, `.`, `0`, `1`, `2`, `4`, `5`, `6`, `8`, `9`, `A`, `B`, `D`, `I`, `K`, `L`, `M`, `P`, `Q`, `S`, `T`, `W`, `b`, `d`, `i`, `k`, `l`, `q`, `s`, `x`.
  - Mines `S` is intentionally HD-flavored as an `80%` snake / `20%` cobra roll, even though ClassicHD used guaranteed snake.
  - Void/backwall gaps are not caused by missing glyph rules; any remaining visual gaps are room-template or tile-art bugs, not unmapped glyphs.

- [x] `PushBlock` is implemented or intentionally replaced.
  - Classic glyph `4` intentionally spawns our implemented `block` entity.
  - The old `PushBlock` archetype remains an unimplemented Classic stub, but Classic glyph data no longer references it.

- [x] `Bones` floor clutter is implemented or intentionally replaced.
  - ClassicHD has real inert `oBones` floor clutter plus loose `oSkull`; `oFakeBones` is the separate skeleton ambush.
  - `Bones` now uses the dedicated `bones` frame data, is inert floor clutter, and breaks from non-stomp hits.
  - `AddMinesTreasure` already uses ClassicHD's `bonesChance = 0` baseline, which still permits normal low-frequency bones spawns.

- [x] Mines ambient enemy odds are audited.
  - Matched against `SpelunkyClassicHD/scripts/scrEntityGen/scrEntityGen.gml`.
  - Ceiling checks match ClassicHD's open-below requirements and now avoid spawning ambient ceiling enemies on occupied spawn points.
  - Odds match ClassicHD: giant spider level pre-roll `1/6`, giant spider ceiling placement `1/40`, dark lamp `1/60`, dark scarab `1/40`, bat `1/60`, spider hang `1/80`, snake `1/60`, caveman `1/800`.
  - Cobra is not part of ClassicHD ambient Mines generation; our cobra remains only as an intentional HD-flavored room glyph roll on Mines `S`, not in ambient spawning.

- [x] Mines floor treasure/object odds are audited.
  - Matched against `SpelunkyClassicHD/scripts/scrTreasureGen/scrTreasureGen.gml`.
  - Open-floor early rolls match: rock `1/100`, pot/jar `1/40`.
  - Alcove rolls match order and odds: cobweb `1/60` or `1/5` near giant spider, box/crate `1/10`, chest `1/15`, damsel `1/8`, skeleton/bones `1/(40 - 2*level)`, gold/gem fallback `1/3`, `1/6`, `1/6`, `1/8`, `1/10`.
  - Tunnel rolls match order and odds: cobweb `1/60` or `1/10` near giant spider, gold `1/4`, gold stack `1/8`, skeleton/bones `1/(80 - level)`, gems `1/8`, `1/9`, `1/10`.
  - Normal floor rolls match: gold `1/40`, gold stack `1/50`, skeleton/bones `1/(140 - 2*level)`.
  - Classic `oCrate` is intentionally represented by our implemented `Box`; box spawn placement is size-aware so it does not break on initial physics correction.
  - Push-block side support is represented by spawned `block` entities, matching ClassicHD's `oBlock` side-support checks.
  - Underground embedded item odds are tracked by the separate embedded treasure checkbox below.

- [x] Embedded treasure odds are audited.
  - Matched against ClassicHD Mines `oBrick/Create_0.gml`.
  - Visible gold roll matches: `n < 20`; visible gold-big roll matches: `n < 30`.
  - ClassicHD used gold vein sprite variants; Splonks now represents mine gold veins as visible embedded payloads over normal dirt so the same embed system can work on non-cave tile families.
  - Embedded gems match ClassicHD order and odds: sapphire `1/100`, emerald `1/120`, ruby `1/140`.
  - Underground item roll matches `1/1200`.
  - `underground_items` pool is uniform across ClassicHD's 19 switch cases; y-offset polish for individual underground item sprite placement remains visual-only, not odds parity.

- [x] Snake pit generation is audited.
  - Matched against ClassicHD `global.probSnakePit = 8`.
  - Placement matches ClassicHD: only side rooms, requires three empty vertical rooms, and can extend to four rooms from the top row when the fourth room is free.
  - ClassicHD room code `7` falls through to the normal drop-room generator; Splonks preserves that as `drop -> snake_pit_top -> snake_pit_bottom`.
  - Snake pit top and bottom room glyph templates match the ClassicHD strings, including ruby and mattock bottom-room treasure.
  - Lowercase `s` matches ClassicHD: `10%` snake, otherwise `50%` solid cave dirt.
  - Uppercase `S` intentionally remains an HD-content deviation: `80%` snake, `20%` cobra.

- [x] Shop generation is audited.
  - Matched against ClassicHD `scrLevelGen`: shops require `currLevel > 1`, roll `rand(1, currLevel) <= 2`, and select one eligible side room adjacent to a main/drop room.
  - `classic_mines_1` therefore correctly has no random shop; Mines 2+ can place shops with ClassicHD's level-dependent chance.
  - Generated shop rooms now emit the same runtime shop concept used by the shop demo: one invisible `Shop` area, linked shopkeeper, owned buyables, buy prompts, theft detection, and tile-break disturbance.
  - Shop signs, store lights, wanted posters, item slots, craps dice, and kissing-shop damsel are resolved from Classic room glyphs.
  - Buy prices use ClassicHD item base costs with the same post-level-2 scaling rule; `Damsel` keeps the current Splonks/demo price until kissing shop economy is revisited.

- [x] HD-style vault generation is added for eligible stages.
  - Vaults are enabled on Mines 1-2+, Jungle, Ice Caves, and Temple.
  - Vaults are intentionally disabled for Mines 1-1, Black Market, Haunted Castle, City of Gold, and Olmec's Lair.
  - Vault rooms emit an invisible disturbed `Shop` area, an owned hostile shopkeeper, and two chests.
  - Vault walls use a dedicated glyph that rolls `1/4` into a pushable `block` entity and otherwise resolves to the stage's themed shop wall tile.

- [ ] Idol/tiki/boulder path is audited.
  - Verify idol spawn odds and room constraints.
  - Verify tiki/boulder trigger path works in normal Mines generation, not only test rooms.

- [ ] Altar generation and sacrifice behavior is audited for Mines.
  - Verify altar spawn odds and room placement.
  - Verify sac altar and topper behavior works when generated normally.

- [ ] Entrances/exits are audited.
  - Verify `BasicExit` conversion and route target.
  - Verify prompt/interaction behavior and depth/route transition.

- [ ] Manual playtest passes.
  - Generate several `classic_mines_1` seeds.
  - Confirm reachable exit, no obvious bad room seams, no broken branch/shop/idol/altar placement, and no unimplemented `NoSprite` entities appearing in normal play.

- [ ] `classic_mines_2` / `1-2` / Mines
  - Needs the same Mines parity audit.
  - Key/chest route must be correct and tested.

- [ ] `classic_mines_3` / `1-3` / Mines
  - Needs the same Mines parity audit.
  - Key/chest route must be correct and tested.

- [ ] `classic_mines_4` / `1-4` / Mines
  - Needs the same Mines parity audit.
  - Route should continue to Jungle once Jungle is validated.

- [ ] `classic_jungle_1` / `2-1` / Jungle
  - Data exists.
  - Needs jungle room pool audit, jungle ambient pass parity, water/lake behavior, piranha/jaws/mantrap/frog/monkey behavior, tree/vine behavior, and hazard odds.

- [ ] `classic_jungle_2` / `2-2` / Jungle
  - Needs Jungle parity.
  - Black Market and Haunted Castle branch exits must be validated.

- [ ] `classic_jungle_3` / `2-3` / Jungle
  - Needs Jungle parity.
  - Return path from Black Market / Haunted Castle must be validated.

- [ ] `classic_jungle_4` / `2-4` / Jungle
  - Needs Jungle parity.
  - Route should continue to Ice Caves once Ice Caves is validated.

- [ ] `classic_ice_caves_1` / `3-1` / Ice Caves
  - Data exists but current check command fails before full route completion because later room pools are incomplete.
  - Needs open abyss generation, platform behavior, UFO/alien/yeti behavior, thin ice, dark fall, alien craft, and correct shop/treasure odds.

- [ ] `classic_ice_caves_2` / `3-2` / Ice Caves
  - Needs Ice Caves parity.

- [ ] `classic_ice_caves_3` / `3-3` / Ice Caves
  - Needs Ice Caves parity.

- [ ] `classic_ice_caves_4` / `3-4` / Ice Caves
  - Needs Ice Caves parity.
  - Moai / special branch behavior must be validated or intentionally deferred.

- [ ] `classic_temple_1` / `4-1` / Temple
  - Data exists.
  - Needs temple room pool audit, temple ambient pass parity, lava behavior, ceiling traps, tomb lords, push blocks, Xoc rooms, altars, and Anubis/sceptre route decisions.

- [ ] `classic_temple_2` / `4-2` / Temple
  - Needs Temple parity.

- [ ] `classic_temple_3` / `4-3` / Temple
  - Needs Temple parity.
  - City of Gold branch requirements must be validated or intentionally adapted.

- [ ] `classic_olmec_lair` / `4-4` / Olmec's Lair
  - Data exists.
  - Needs Olmec room shape/pool audit, boss behavior, exit/win behavior, treasure behavior, and generation that does not rely on normal room graph assumptions if that is inaccurate.

## Branch / Bonus Stages

- [ ] `classic_black_market` / Black Market
  - Data exists.
  - Needs shop layout parity, shopkeeper behavior stress test, black market branch state, and return route validation.

- [ ] `classic_haunted_castle` / Haunted Castle
  - Data exists.
  - Needs undead/castle entity behavior, room pools, branch entrance/exit behavior, and ambient pass parity.

- [ ] `classic_city_of_gold` / City of Gold
  - Data exists.
  - Needs City of Gold material/drop parity, Xoc/Book route decisions, lava/temple hazard behavior, and branch exit validation.

## Deferred / Not Targeted

- [ ] Worm
  - Deferred by design.

- [ ] Hell
  - Low priority. Not required for engine-demo capstone.

- [ ] Yama's Throne
  - Deferred by design.

## Current Known Blockers

- [x] `build/splonks-cpp --check-classic-quest-stagegen` runs through all configured Classic stages.
  - This now uses stage-specific hard pool-count checks; it still does not prove enemy odds, treasure odds, or entity behavior parity.

- [ ] Several Classic entities intentionally exist as `NoSprite` unimplemented archetype stubs.
  - These keep data spawnable, but stages using them cannot be marked complete until behavior is implemented or intentionally substituted.

- [ ] Water and lava are currently tile mappings, but need final gameplay behavior.
  - Likely needs a shop-area-style watcher entity or a lightweight fluid area system so broken containers can drain/clear correctly.
