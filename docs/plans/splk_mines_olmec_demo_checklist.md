# Splk Mines / Olmec Demo Checklist

Purpose: finish `SplkMines1`, `SplkMines2`, `SplkMines3`, and `Olmec/Boss` as the engine demo baseline before the game diverges from Spelunky.

Status legend:
- `[ ]` missing
- `[-]` present but rough / partial / needs integration
- `[x]` done enough for the demo

## Core Demo Scope

- [ ] Make the playable progression hit `Olmec/Boss` for the demo path instead of continuing into the later biome chain.
- [ ] Implement the `Boss` stage generator / room layout.
- [ ] Implement `Olmec` as the actual boss encounter.
- [ ] Add a level editor.

## Mines 1 / 2 / 3 Generation

- [x] Basic authored mines generation exists.
- [x] Entrance / exit path generation exists.
- [x] Idol / tiki / boulder path exists.
- [x] Udjat key chest chain exists.
- [x] Sac altar exists.
- [x] Shops can spawn in authored mines layouts.
- [ ] Hook up snake pit generation.
  - Templates exist, but the layout generator currently never assigns the snake pit room codes.
- [-] Increase stage-specific variation between `SplkMines1`, `SplkMines2`, and `SplkMines3`.
  - Right now they mostly share one generator with limited tuning.
- [-] Audit authored room variety versus target demo feel.
  - This is the pass for "does mines 1/2/3 actually feel feature-complete", not just "does it technically generate".

## Shops

- [x] Real shop engine pieces exist.
  - `Shop` area ent
  - buyable state
  - disturbance / anger plumbing
  - shopkeeper ent
- [ ] Replace mines shop generation with the real shop area pipeline.
  - Generated mines shops should spawn a real `Shop` area and register buyables/children against it.
- [ ] Make generated mines shops use the same ownership / stealing / anger logic as the demo shop path.
- [-] Audit shopkeeper behavior once mines shops are integrated.
- [-] Audit special shop cases and prompts after integration.

## Mines Enemies / Hazards

- [x] Bat
- [x] Spider / hanging spider
- [x] Rage spider variants
- [x] Giant spider
- [x] Snake
- [x] Caveman
- [x] Arrow trap
- [ ] Skeleton
- [ ] Scorpion
  - `Scarab` exists in code; `Scorpion` does not.
- [ ] Large spitting snake / cobra variant
- [ ] Spit proj / attack behavior for the spitter
- [ ] Slow ghost
  - Replace the current placeholder-ish `GhostBall` with the real pressure mechanic, or implement a mines-demo equivalent good enough to stand in for it.

## Items / Pickups / Weapons

- [x] Pots / chests / crates exist
- [x] Gold / gems / idol / key chest / Udjat path exist
- [ ] Do a full item implementation audit and close missing behavior
- [ ] Verify `Teleporter`
- [ ] Verify `Cape`
- [ ] Verify `Parachute`
- [ ] Verify `Mattock`
- [ ] Verify `Dice` / gambling flow
- [ ] Verify shop-sold items all work correctly after real shop integration
- [-] Audit passive item effects as a batch
  - This is the "do they merely exist as pickups, or do they really do the intended thing" pass.

## Olmec / Boss

- [ ] Boss stage layout
- [ ] Olmec ent
- [ ] Olmec movement / stomp / crush behavior
- [ ] Tile destruction / terrain interaction for the fight
- [ ] Fight completion flow
- [ ] Post-fight progression / demo completion behavior
- [-] Boss camera / pres pass

## Physics / Feel Pass

- [ ] Gravity tuning pass
- [ ] Jump tuning pass
- [ ] Walk / run acceleration pass
- [ ] Air control pass
- [ ] Friction / slide pass
- [ ] Short-hop / jump-cut validation
- [ ] Coyote / jump-buffer validation
- [ ] Ledge / hang validation
- [ ] Rope / climb validation
- [ ] Boulder / heavy-object feel validation
- [ ] Moving platform carry validation
- [ ] Regression room for movement feel comparisons

## Nice To Lock Down Before Calling The Demo "Done"

- [ ] Make mines content coverage explicit with a test checklist
  - shop
  - idol + boulder
  - Udjat key chest
  - snake pit
  - altar / sacrifice
  - damsel
  - ghost pressure
- [ ] Make a boss test room for rapid Olmec iteration
- [ ] Make a movement / physics test room for gravity / jump / friction comparisons

## Suggested Work Order

- [ ] 1. Hook up snake pit generation
- [ ] 2. Integrate real mines shops
- [ ] 3. Add skeleton
- [ ] 4. Add scorpion
- [ ] 5. Add large spitting snake / cobra and spit proj
- [ ] 6. Add slow ghost
- [ ] 7. Route the demo progression to `Boss`
- [ ] 8. Implement Olmec stage + boss
- [ ] 9. Do the gravity / jump / friction / feel pass
- [ ] 10. Close the remaining item implementation gaps
