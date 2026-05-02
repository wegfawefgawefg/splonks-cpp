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

## Trigger Notes

- Exact authored tile destruction should use `StageTileTrigger`. Shop wall/vault vandalism now follows this path and emits stagegen annotations on each trigger tile.
- Entity-owned `on_area_enter`, `on_area_exit`, and `on_area_tile_changed` callbacks remain useful for moving/dynamic detectors.
- Future tile-location enter/exit triggers may be worth adding if a feature needs exact authored tile regions for pressure plates, tile-bound prompts, camera/music zones, fluid volumes, or shop threshold lines. Do not add that system until a real use case needs it.

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
  - Removed the old unimplemented `PushBlock` placeholder type/archetype.

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
  - Shop wall/vault vandalism uses exact `StageTileTrigger` tile ownership, and stagegen annotations mark every vandalism trigger tile.
  - Shop signs, store lights, wanted posters, item slots, craps dice, and kissing-shop damsel are resolved from Classic room glyphs.
  - Buy prices use ClassicHD item base costs with the same post-level-2 scaling rule; `Damsel` keeps the current Splonks/demo price until kissing shop economy is revisited.

- [x] HD-style vault generation is added for eligible stages.
  - Vaults are enabled on Mines 1-2+, Jungle, Ice Caves, and Temple.
  - Vaults are intentionally disabled for Mines 1-1, Black Market, Haunted Castle, City of Gold, and Olmec's Lair.
  - Vault rooms emit an invisible disturbed `Shop` area, an owned hostile shopkeeper, and two chests.
  - Vault walls use a dedicated glyph that rolls `1/4` into a pushable `block` entity and otherwise resolves to the stage's themed shop wall tile.

- [x] Idol/tiki/boulder path is audited.
  - Matched Mines idol spawn constraints against ClassicHD `scrRoomGen`: side-room only, not bottom room row, one idol room max, and `1/10` sequential roll across eligible side rooms.
  - Mines idol room now uses a dedicated `idol` room pool and `idol` layout pass instead of living in the normal side-room pool, preventing duplicate idols and bottom-row idol rooms.
  - `I` and `B` glyphs link `GoldIdol` to `GiantTikiHead` via authored spawn indices in normal generation, not only test rooms.
  - Tiki/boulder runtime behavior exists: idol movement disturbs the tiki head, plays windup, spawns a boulder, and uses the current boulder rolling/shake/tile-break behavior.

- [x] Dart/arrow trap generation and behavior is audited.
  - `arrow_trap_conversion` turns eligible `Block` spawns into tile-sized impassable `ArrowTrap` entities with left/right facing.
  - ClassicHD trap sensor behavior was matched: horizontal strip from the trap face toward the first solid, capped at `96 px`, minimum `32 px`, one-shot fire on a moving entity in the strip.
  - `Arrow` is now a normal content entity with projectile damage, gravity, tile sticking, and entity contact cleanup.

- [x] Altar generation and sacrifice behavior is audited for Mines.
  - Matched Mines altar placement against ClassicHD `scrRoomGen`: side rooms only, `currLevel > 1`, one altar max, sequential `1/16` roll across eligible side rooms.
  - `classic_mines_1` correctly cannot generate a random altar because ClassicHD gates altars to level 2+.
  - Empirical sample with `build/splonks-cpp --sample-classic-mines-altars 1000`: `classic_mines_1` `0/1000`, `classic_mines_2` `225/1000`, `classic_mines_3` `269/1000`, `classic_mines_4` `275/1000`.
  - The altar room is now a dedicated `altar` pool selected by the layout pass, not a normal side-room template with equal weight.
  - Generated `x` glyphs spawn linked `SacAltar` left/right halves plus a `SacAltarTopper`; authored spawn-link resolution gives generated altars the same owner/topper relationship used by the altar test room.
  - Sacrifice runtime is content-owned by `sac_altar.cpp`: only the owner left half consumes grounded eligible victims, awards run-level favor, plays sacrifice effects/audio, triggers the topper sacrifice animation, and grants configured reward tiers.

- [x] Entrances/exits are audited.
  - Classic glyph `9` matches the intended split: start rooms leave one `Entrance` tile for player placement; end rooms spawn one `BasicExit` entity and leave air under it.
  - `convert_exit_tiles` remains as a safety pass for legacy `Exit` tiles, but normal Classic room glyph resolution already emits `BasicExit` spawns directly.
  - Runtime exit behavior is owned by `BasicExit`: overlap prompt, RB interaction, route permission check, and transition queueing.
  - Route data is owned by quest/stage data; `classic_mines_1 -> 2 -> 3 -> 4 -> classic_jungle_1` is declared in `assets/quests/classic/quest.yaml`.
  - `build/splonks-cpp --check-classic-quest-stagegen` now hard-validates exactly one entrance tile, exactly one default `BasicExit`, and every generated exit id resolving to declared stage route data for normal non-Olmec Classic stages.
  - Olmec remains separately unchecked because its entrance/win path is not a normal Classic room-graph entrance.

- [x] Udjat key/chest generation odds are audited.
  - ClassicHD only attempts the Udjat chain if it has not already been made.
  - ClassicHD Mines odds are level 2: `1/3`, level 3: `1/2`, level 4: guaranteed.
  - Splonks now matches that run-state shape: stagegen receives mutable quest state, skips if `made_udjat_eye` or `has_udjat_eye` is already set, and marks `made_udjat_eye` when it successfully places both key chest and key.
  - Pickup behavior is tracked separately under passive item behavior.

- [ ] Player core survival mechanics are audited.
  - [x] Fall timer tracking is correct for player characters.
  - [x] Fall damage/stun thresholds match the intended Classic/HD feel.
  - [x] Parachute suppresses fall damage only after valid deploy conditions.
  - [x] Spike tile damage interacts correctly with spike shoes.
  - [x] Spike hitbox and ladder/rope/climb interactions match Classic/HD edge cases.
  - [ ] Stomp damage and stomp immunity are consistent across enemies, props, and carried/thrown items.
    - Current details: stomp requires `can_stomp`, normal condition, downward velocity, not held, and not hanging; targets require `can_be_stomped`, non-impassable, collidable, normal condition, and not hanging.
    - Base stomp damage is `1`; spike shoes raise stomp damage through the effect modifier path.
    - Fixed in current pass: generated chest/key chest placement no longer starts half embedded, and thrown chest/key chest projectile contact now applies `1` damage.
    - Remaining audit: verify every intended prop/item/enemy archetype has correct `can_be_stomped`/`can_stomp` flags.
  - [x] Crush/telefrag/explosion deaths still route through normal death callbacks so favor, meathead, and effects work.

- [x] Spelunky player physics are audited.
  - [x] Run acceleration, max speed, ground friction, and turnaround feel match the target Spelunky reference.
  - [x] Jump impulse, variable jump hold, gravity, max fall speed, and coyote timing are tuned.
  - [x] Ladder/rope attach, detach, top latch, climb speed, and climb animation are correct.
    - Rope deployment extends from the lowest connected climbable tile when it hits an existing rope/ladder chain.
  - [x] Ledge/wall hang probes, glove wall hang, and hang release behavior are correct.
    - Auto corner grab and glove wall hang share capture logic without forcing glove wall hang every frame.
    - Hang coyote refreshes to `6` frames while normal grounded coyote remains separately tuned.
  - [x] Throw strength, pickup/carry movement penalties, and held-item aiming feel correct.
  - [x] Camera follow/listener behavior does not hide gameplay issues or introduce jitter.
    - Camera follow tracks player visual center; audio listener is explicit world state and positional emitters update against it each frame.

- [x] Passive item behavior is audited.
  - [x] `Gloves`: wall hang behavior works and does not override normal ledge/climb rules incorrectly.
    - ClassicHD: gloves let the player hang while falling, holding into a solid wall, and side probes hit a solid wall; down+jump release uses a 10-frame hang cooldown.
    - Splonks: gloves are a passive item routed through the shared hang path, require falling plus directional wall input, zero vertical motion while hanging, and use the same 10-frame glove drop cooldown.
  - [x] `Spectacles`: embedded treasure reveal works.
    - ClassicHD: spectacles set `hasSpectacles`, and buried treasure becomes visible when `hasSpectacles` or `hasUdjatEye` is true.
    - Splonks: spectacles are a passive pickup, and embedded treasure overlays reveal when any active entity has `Spectacles` or `UdjatEye`.
  - [x] `UdjatEye`: key chest pickup sets quest state and reveals embedded treasure.
    - ClassicHD: key chest creates Udjat Eye, pickup sets `hasUdjatEye`, and hidden treasure becomes visible.
    - Splonks: key chest opens into `UdjatEye`, pickup sets the passive and Classic quest `has_udjat_eye`, and embedded treasure reveal shares the same path as spectacles.
  - [x] `Compass`: screen-space arrow indicator exists and points to the default exit.
  - [x] `Mitt`: thrown item behavior is implemented and tuned.
  - [x] `Paste`: pickup converts the bomb tool into sticky bombs, and later bomb refills route into the sticky bomb slot.
  - [x] `SpringShoes`: jump height boost is implemented and tuned.
  - [x] `SpikeShoes`: spike immunity and stomp damage boost are implemented and tuned.
  - [x] `Parachute`: single-use pickup, deploy speed threshold, visual placement, and cleanup are correct.
  - [x] `Meathead`: Splonks replacement for Kapala; should appear through Classic sacrifice rewards and stay documented as an intentional adaptation.

- [x] Back items and movement gear are audited.
  - [x] `Cape`: pickup/equip/use behavior exists and matches intended glide/slowfall behavior.
  - [x] `JetPack`: pickup/equip/use, fuel feel, explosion damage, and shop/loot placement are correct.
  - [x] `TeleporterBackpack`: intentionally Splonks-specific; excluded from Classic pools unless explicitly wanted.

- [x] Tools, weapons, and held item behavior are audited.
  - [x] `BombBox` and `BombBag`: add bombs to the bomb tool slot correctly, including empty-slot acquisition.
    - BombBox adds `12`; BombBag adds `3`; if sticky bombs are owned, bomb refills prioritize that slot.
  - [x] `RopePile`: adds ropes to the rope tool slot correctly, including empty-slot acquisition.
    - RopePile adds `3` through the shared tool inventory path.
  - [x] `Mattock`: dig probes, durability, entity hits, sounds, and wrap/border behavior are correct.
  - [x] `Machete`: swing damage, thrown damage, corpse-sac interaction, and altar cash-in are correct.
  - [x] `Pistol`: firing, projectiles, ammo/reload behavior if any, and shop/loot placement are correct.
  - [x] `Bow`: 8-way aim, aimed held rotation, arrow ammo HUD, no-gravity-until-contact shots, loose/stuck arrow reload, and shop/loot placement are correct.
  - [x] `Shotgun`: intentionally out of scope for now; Classic pools substitute implemented weapons instead.
  - [x] `WebCannon`: webball flight, web placement, web decay, and web interaction are correct.
  - [x] `Teleporter`: 8-way target probes, telefrag/splat, wall death, shake, and item/player visual effects are correct.

- [ ] Loot/shop/item pools are audited.
  - [ ] Chest random item pool matches intended Classic/HD item availability.
  - [x] Box contents match intended ClassicHD crate odds.
    - Uses ClassicHD's actual open-crate sequential roll order, with `Shotgun` intentionally substituted by implemented `Pistol`.
    - Final common fallback matches ClassicHD and the HD/2 crate shape: `1/2` `RopePile`, otherwise guaranteed `BombBag`.
  - [ ] Passive item generation odds are audited, including Spectacles in shops, crates/chests, sacrifice rewards, and generated loot.
  - [ ] Shop category pools match ClassicHD categories: general, bomb, weapon, rare, clothing, craps, and kissing.
    - Current YAML shop pools are weighted approximations, not exact sequential ClassicHD roll logic.
  - [ ] Sacrifice reward pool and favor thresholds are intentionally documented versus Classic/HD.
  - [ ] Items that are intentionally Splonks-only are excluded from Classic generation unless explicitly marked as an adaptation.

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
  - Needs jungle room pool audit, jungle ambient pass parity, jaws/frog behavior, tree/vine behavior, and hazard odds.
  - Water is tile-based with rendered surface tops, optional drain-support triggers, and a focused Water/Piranha debug room.
  - Piranha behavior is implemented and uses water-constrained swimming.
  - Monkey behavior is implemented and playtested in the monkey debug room.

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
