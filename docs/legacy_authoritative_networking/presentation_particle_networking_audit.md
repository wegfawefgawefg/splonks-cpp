# Presentation Particle Networking Audit

Goal: gameplay-result visuals that are produced only on the coordinator must arrive through the presentation command lane. Do not replicate individual particles as durable world state.

## Synced Through Presentation Commands

- [x] Explosion burst particles and explosion transient light.
- [x] Pistol muzzle smoke, impact particles, and muzzle transient light.
- [x] Treasure pickup sparkles and pickup transient light.
- [x] Baseball bat trail ribbon.
- [x] Teleporter split/merge particles.
- [x] Jetpack smoke.

## Direct Particle Adds Left To Classify

- [ ] `src/entities/block.cpp`: break shards and smoke.
- [ ] `src/entities/boulder.cpp`: rolling/impact smoke and shards.
- [ ] `src/entities/chest.cpp`: chest sparkle.
- [ ] `src/entities/cobra.cpp`: cobra spit/contact particle.
- [ ] `src/entities/damsel.cpp`: kiss particle.
- [ ] `src/entities/door.cpp`: trap-door smoke and shards.
- [ ] `src/entities/flesh_guy.cpp`: flesh guy effect particle.
- [ ] `src/entities/gold_idol.cpp`: idol particle.
- [ ] `src/entities/mattock.cpp`: dig sparks and smoke.
- [ ] `src/entities/meathead.cpp`: scripted popup.
- [ ] `src/entities/moving_platform.cpp`: platform shard.
- [ ] `src/entities/player.cpp`: player smoke.
- [ ] `src/entities/sac_altar.cpp`: altar smoke, spark, and blood.
- [ ] `src/entities/sac_altar_topper.cpp`: topper smoke.
- [ ] `src/entities/skeleton.cpp`: wake/death smoke.
- [ ] `src/entities/teleporter.cpp`: local teleporter particle at line 359; split/merge presentation is already synced.
- [ ] `src/entities/web_cannon.cpp`: web cannon particle.
- [ ] Decide for each remaining call whether it is local-only ambience, deterministic entity-step presentation that runs on all peers, or coordinator-only gameplay presentation that needs a scripted presentation command.
- [ ] Avoid syncing high-frequency dust every frame unless the source gameplay only runs on the coordinator and the visual materially affects feedback.

## Rule

- Durable objects use entity/tile/fluid/stage-light messages.
- Short-lived visuals use presentation commands.
- Local-only decorative dust may stay local if it is not tied to a coordinator-only result.
