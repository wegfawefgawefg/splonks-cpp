# Terminology Shortening Plan

Goal: reduce high-frequency long identifiers across code and docs without changing behavior. These should be pure rename commits, ideally one mapping or closely related group per commit when the diff is large.

## Decided Mappings

1. `Archetype` -> `Spec`
   - Examples: `EntityArchetype` -> `EntitySpec`, `GetEntityArchetype` -> `GetEntitySpec`, `TileArchetype` -> `TileSpec`.

2. `Entity` -> `Ent`
   - Use broadly only if we commit to the convention project-wide.
   - Examples: `entity_idx` -> `ent_idx`, `other_entity` -> `other_ent`.

3. `FrameData` -> `AFrame`
   - Means authored frame data, not simulation frame.
   - Examples: `FrameDataAnimator` -> `AFrameAnimator` or a later refined name, `frame_data_ids` -> `aframe_ids`.

4. `Animation` -> `Anim`
   - Examples: `animation_id` -> `anim_id`, `TrySetAnimation` -> `TrySetAnim`.

5. `Presentation` -> `Pres`
   - First check whether lockstep removed or made some presentation-net names obsolete.

6. `Coordinator` -> `Host`
   - Do not use `Coord`; it conflicts mentally with coordinate.
   - Some old coordinator names may disappear entirely under lockstep rather than being renamed.

7. `Deterministic` -> `Det`
   - Best for tests/helpers/docs where the abbreviation stays clear.

8. `Attachment` -> `Attach`
   - Examples: `AttachmentMode` -> `AttachMode`, `attachment_mode` -> `attach_mode`.

9. `Projectile` -> `Proj`
   - Use `proj` as the standard abbreviation for projectile.
   - Examples: `projectile_contact_timer` -> `proj_contact_timer`, `projectile_contact_damage_amount` -> `proj_hit_damage`.

10. `ProjectileContact` -> `ProjContact` or `ProjHit`
   - Prefer `ProjHit` for damage/result fields.
   - Prefer `ProjContact` for collision/contact state.

11. `DamageVulnerability` -> `DamageVuln`

12. `ContactResolution` -> `ContactResult`
   - Keep `Contact`, not `Touch`, because contact is standard physics/collision vocabulary.

13. `StageEntitySpawn` -> `EntSpawn`
   - It is entity-only spawn metadata. If later generalized beyond entities, reconsider `SpawnSpec`.

14. `TemplateTile` -> `MetaTile`
   - It is a symbolic tile resolved into a real `Tile`.

15. `LeftOrRight` -> `Side`
   - Examples: `LeftOrRight::Left` -> `Side::Left`.

16. `PlayerInputFrame` -> `InputFrame`
   - Input can drive any controlled entity/player slot.

17. `EntityManager` -> `EntPool`
   - Field should become `state.ents`.
   - Type example: `EntityManager` -> `EntPool`.

## Rollout Rules

- Keep each rename behavior-free.
- Update code, tests, docs, comments, debug UI labels, CLI output, and filenames where appropriate.
- Prefer compile-verified mechanical renames over partial hand edits.
- Run at least:
  - `cmake --build build --target splonks-cpp -j 8`
  - `./build/splonks-cpp --check-deterministic-replay-smoke`
  - `./build/splonks-cpp --check-input-lockstep-smoke`
  - `./build/splonks-cpp --check-state-equality-smoke`
- Avoid mixing these renames with rollback/network behavior changes.
