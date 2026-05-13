# Pres Particle Networking Audit

Goal: gameplay-result visuals that are produced only on the host must arrive through the pres command lane. Do not replicate individual particles as durable world state.

## Synced Through Pres Commands

- [x] Explosion burst particles and explosion transient light.
- [x] Pistol muzzle smoke, impact particles, and muzzle transient light.
- [x] Treasure pickup sparkles and pickup transient light.
- [x] Baseball bat trail ribbon.
- [x] Teleporter split/merge particles.
- [x] Jetpack smoke.

## Direct Particle Adds Left To Classify

- [ ] `src/ents/block.cpp`: break shards and smoke.
- [ ] `src/ents/boulder.cpp`: rolling/impact smoke and shards.
- [ ] `src/ents/chest.cpp`: chest sparkle.
- [ ] `src/ents/cobra.cpp`: cobra spit/contact particle.
- [ ] `src/ents/damsel.cpp`: kiss particle.
- [ ] `src/ents/door.cpp`: trap-door smoke and shards.
- [ ] `src/ents/flesh_guy.cpp`: flesh guy effect particle.
- [ ] `src/ents/gold_idol.cpp`: idol particle.
- [ ] `src/ents/mattock.cpp`: dig sparks and smoke.
- [ ] `src/ents/meathead.cpp`: scripted popup.
- [ ] `src/ents/moving_platform.cpp`: platform shard.
- [ ] `src/ents/player.cpp`: player smoke.
- [ ] `src/ents/sac_altar.cpp`: altar smoke, spark, and blood.
- [ ] `src/ents/sac_altar_topper.cpp`: topper smoke.
- [ ] `src/ents/skeleton.cpp`: wake/death smoke.
- [ ] `src/ents/teleporter.cpp`: local teleporter particle at line 359; split/merge pres is already synced.
- [ ] `src/ents/web_cannon.cpp`: web cannon particle.
- [ ] Decide for each remaining call whether it is local-only ambience, det ent-step pres that runs on all peers, or host-only gameplay pres that needs a scripted pres command.
- [ ] Avoid syncing high-frequency dust every frame unless the source gameplay only runs on the host and the visual materially affects feedback.

## Rule

- Durable objects use ent/tile/fluid/stage-light messages.
- Short-lived visuals use pres commands.
- Local-only decorative dust may stay local if it is not tied to a host-only result.
