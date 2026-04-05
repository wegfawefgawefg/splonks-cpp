#Literal Port Checklist

Goal: port `splonks-rs/src` to `splonks-cpp/src` as literally as possible, file by file, without
implied coverage.

Status meanings:
- `todo`: not started
- `in_progress`: active work
- `ported`: translated, not yet runtime-checked
- `verified`: translated and exercised enough to trust the landing

Rules:
- Preserve Rust file/module boundaries wherever practical.
- Preserve naming, control flow, and behavior before cleanup/refactor.
- Record any unavoidable C++ deviation in the notes column.
- Do not silently skip files. Every source file gets a line item.

## Foundation

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/direction.rs` | `src/direction.hpp` + `src/direction.cpp` | Uses a tiny `std::mt19937` helper to preserve the Rust random-pick behavior. |
| ported | `src/utils.rs` | `src/utils.hpp` + `src/utils.cpp` | Added shared `src/math_types.hpp` as the minimal stand-in for `glam::Vec2` and `glam::IVec2`. |
| ported | `src/sid.rs` | `src/sid.hpp` + `src/sid.cpp` | Uses a temporary linear scan backend instead of `rstar::RTree`; query behavior is preserved first, backend can be swapped later. |
| ported | `src/tile.rs` | `src/tile.hpp` + `src/tile.cpp` | Preserves the Rust random range exactly, including the unreachable ladder/spikes branches behind `0..=5`. |
| in_progress | `src/room.rs` | `src/room.hpp` + `src/room.cpp` | Room/template enums, template paste, random template resolution, and `gen_room` dispatch are ported. |
| ported | `src/settings.rs` | `src/settings.hpp` + `src/settings.cpp` | |
| in_progress | `src/inputs.rs` | `src/inputs.hpp` + `src/inputs.cpp` | Input state structs, menu debounce timers, SDL keyboard polling, SDL gamepad init, pressed-edge helpers, and per-mode input dispatch are ported. Exact SDL-side controller parity still needs more cleanup. |
| ported | `src/sprite.rs` | `src/sprite.hpp` + `src/sprite.cpp` | Uses `nlohmann::json` from local workspace and a peeled-out `Origin` support type instead of waiting for full `entity.rs`. |
| ported | `src/entity_display_states.rs` | `src/entity_display_states.hpp` + `src/entity_display_states.cpp` | Uses a temporary `EntityDisplayInput` support struct instead of the full `Entity` type. |

## World And Game State

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| in_progress | `src/entity.rs` | `src/entity.hpp` + `src/entity.cpp` | Struct shape, defaults, and geometry/state helpers are ported. The old top-left / inclusive-bounds gameplay semantics and grounded/hang helpers are back in place and runtime-validated, but the full file still carries broader non-physics port cleanup. |
| ported | `src/entity_manager.rs` | `src/entity_manager.hpp` + `src/entity_manager.cpp` | Uses raw pointers / `std::optional` instead of Rust `Option<&T>` / `Option<&mut T>`, but preserves the visible manager behavior. |
| in_progress | `src/state.rs` | `src/state.hpp` + `src/state.cpp` | State struct, defaults, mode switching, SID rebuild, `is_stage_won`, and special-effect stepping are ported. |
| in_progress | `src/stage.rs` | `src/stage.hpp` + `src/stage.cpp` | Tile query/update helpers, blank stage, room-layout/path generation, and stage generator wiring are ported. |
| ported | `src/stage_init.rs` | `src/stage_init.hpp` + `src/stage_init.cpp` | Player spawn, entity clear, and entrance-door placement path are ported. |
| verified | `src/step.rs` | `src/step.hpp` + `src/step.cpp` | Step loop, mode dispatch, stage progression mapping, and Rust comments are ported. Raylib handle/thread parameters were collapsed out of the C++ signature. Fixed-step gameplay semantics were restored from `splonks-old` and runtime-validated. |
| in_progress | `src/step_entities.rs` | `src/step_entities.hpp` + `src/step_entities.cpp` | Rust comments and dispatch structure are ported. All current entity branches are wired; what remains is parity cleanup rather than missing modules. |
| ported | `src/on_damage_effects.rs` | `src/on_damage_effects.hpp` + `src/on_damage_effects.cpp` | |

## Rendering And Runtime

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| in_progress | `src/graphics.rs` | `src/graphics.hpp` + `src/graphics.cpp` | Data types, sprite metadata loading, screen-space helpers, tile/font helper functions, SDL_ttf font caching, and SDL texture loading are ported. Shader parity and the remaining backend/runtime cleanup are still pending. |
| ported | `src/audio.rs` | `src/audio.hpp` + `src/audio.cpp` | Public surface preserved; backend is currently a no-op SDL-side placeholder that validates asset files and stores playback state only. |
| in_progress | `src/render.rs` | `src/render.hpp` + `src/render.cpp` | SDL render dispatcher exists and is split back out by Rust file boundaries. Menus and stage/game-over/win text render through SDL_ttf, and title parallax now uses real textures. Shader parity and some layout cleanup are still pending. |
| in_progress | `src/render_tiles_and_entities.rs` | `src/render_tiles_and_entities.hpp` + `src/render_tiles_and_entities.cpp` | Texture-backed tile/entity/effect rendering is in place. The remaining gap is parity cleanup around missing sprite assets and deeper runtime behavior. |
| in_progress | `src/render_ui.rs` | `src/render_ui.hpp` + `src/render_ui.cpp` | HUD icon/count layout and stage-type text now follow the Rust structure. Remaining gaps are asset completeness and final visual parity cleanup. |
| in_progress | `src/render_debug.rs` | `src/render_debug.hpp` + `src/render_debug.cpp` | Control-help output, stage layout, and room overlay helpers are ported with SDL primitive drawing and SDL_ttf labels. |

## Menus

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| in_progress | `src/menu_title.rs` | `src/menu_title.hpp` + `src/menu_title.cpp` | Menu enums, option-name mapping, input state machine, Rust-style text layout, and title parallax rendering are ported. Remaining work is exact input/visual parity cleanup. |
| in_progress | `src/menu_settings.rs` | `src/menu_settings.hpp` + `src/menu_settings.cpp` | Menu enums, option-name mapping, input state machine, and Rust-style text layout are ported. |
| in_progress | `src/menu_video.rs` | `src/menu_video.hpp` + `src/menu_video.cpp` | Menu enums, resolution list, option-name mapping, SDL-side apply behavior, and Rust-style text layout are ported. |

## Stage Generation

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/stage_gen/mod.rs` | `src/stage_gen/mod.hpp` + `src/stage_gen/mod.cpp` | Literal module entrypoint is in place. |
| ported | `src/stage_gen/cave.rs` | `src/stage_gen/cave.hpp` + `src/stage_gen/cave.cpp` | Bulk-aligned with `test.cpp`, matching the near-identical Rust source. The remaining stand-in branches are also stand-ins in Rust. |
| ported | `src/stage_gen/test.rs` | `src/stage_gen/test.hpp` + `src/stage_gen/test.cpp` | Helper inventory and current branch/template selection are ported. The remaining stand-in branches are also stand-ins in Rust. |

## Systems

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/systems/mod.rs` | `src/systems/mod.hpp` + `src/systems/mod.cpp` | Literal module entrypoint includes the translated controls module. |
| verified | `src/systems/controls.rs` | `src/systems/controls.hpp` + `src/systems/controls.cpp` | Old control-facing player intent flags were restored to support the known-good physics path, and the movement path is runtime-validated. |

## Entities

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/entities/mod.rs` | `src/entities/mod.hpp` + `src/entities/mod.cpp` | Literal module entrypoint exists and includes the translated entity headers. |
| in_progress | `src/entities/common.rs` | `src/entities/common.hpp` + `src/entities/common.cpp` | Core stun/travel-sound/euler/gravity/grounded/damage/jump helpers are ported, plus tile/entity collision, hurt-on-contact, death/deactivate, projectile hurt, spikes, and grounded-on-entities paths. The old fixed-step physics/collision semantics from `splonks-old` are restored and runtime-validated. Some later Rust helpers like explosion/fall helpers are still not surfaced in C++. |
| in_progress | `src/entities/player.rs` | `src/entities/player.hpp` + `src/entities/player.cpp` | `set_entity_player`, active player logic, pickup handling, display-state routing, sprite sync, and physics step are ported. Old movement/collision behavior from `splonks-old` is restored and runtime-validated, including hang/climb/friction/stomp behavior. Large Rust commented-out action sections remain commented-out here too, and bomb/rope/attack/push paths still depend on the unported entity modules. |
| ported | `src/entities/block.rs` | `src/entities/block.hpp` + `src/entities/block.cpp` | |
| ported | `src/entities/bat.rs` | `src/entities/bat.hpp` + `src/entities/bat.cpp` | |
| ported | `src/entities/baseball_bat.rs` | `src/entities/baseball_bat.hpp` + `src/entities/baseball_bat.cpp` | |
| ported | `src/entities/breakaway_container.rs` | `src/entities/breakaway_container.hpp` + `src/entities/breakaway_container.cpp` | |
| ported | `src/entities/rope.rs` | `src/entities/rope.hpp` + `src/entities/rope.cpp` | |
| ported | `src/entities/ghost_ball.rs` | `src/entities/ghost_ball.hpp` + `src/entities/ghost_ball.cpp` | |
| ported | `src/entities/jetpack.rs` | `src/entities/jetpack.hpp` + `src/entities/jetpack.cpp` | |
| ported | `src/entities/money.rs` | `src/entities/money.hpp` + `src/entities/money.cpp` | |
| ported | `src/entities/rock.rs` | `src/entities/rock.hpp` + `src/entities/rock.cpp` | |
| ported | `src/entities/bomb.rs` | `src/entities/bomb.hpp` + `src/entities/bomb.cpp` | |
| ported | `src/entities/mouse_trailer.rs` | `src/entities/mouse_trailer.hpp` + `src/entities/mouse_trailer.cpp` | |

## Special Effects

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/special_effects/mod.rs` | `src/special_effects/mod.hpp` + `src/special_effects/mod.cpp` | Literal module entrypoint includes the translated special effect files. |
| ported | `src/special_effects/special_effect.rs` | `src/special_effects/special_effect.hpp` + `src/special_effects/special_effect.cpp` | Rust trait + exported macro translated to a plain abstract interface plus free helper functions. |
| ported | `src/special_effects/static_effect.rs` | `src/special_effects/static_effect.hpp` + `src/special_effects/static_effect.cpp` | |
| ported | `src/special_effects/dynamic_effect.rs` | `src/special_effects/dynamic_effect.hpp` + `src/special_effects/dynamic_effect.cpp` | |
| ported | `src/special_effects/spline_effect.rs` | `src/special_effects/spline_effect.hpp` + `src/special_effects/spline_effect.cpp` | |
| ported | `src/special_effects/ultra_dynamic_effect.rs` | `src/special_effects/ultra_dynamic_effect.hpp` + `src/special_effects/ultra_dynamic_effect.cpp` | |

## Entrypoint

| Status | Rust source | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/main.rs` | `src/main.cpp` | Startup/loop/shutdown now live directly in `main.cpp` instead of the temporary `App` wrapper. |

## Non-Rust Source Assets Under `src/`

These are not Rust modules, but they are still part of the source tree and should be tracked for
parity.

| Status | Source asset | C++ target | Notes |
| --- | --- | --- | --- |
| ported | `src/shaders/vert.fs` | `src/shaders/vert.fs` | Copied forward as-is. |
| ported | `src/shaders/grayscale.fs` | `src/shaders/grayscale.fs` | Copied forward as-is. |

## Explicitly Excluded From The Port Checklist

- `src/.hypothesis/...`: not game source, do not port
