# State / PlayState Replay Split

`State` currently mixes application state and gameplay simulation state. That makes
replay snapshots harder to maintain because every new field added to `State` has
to be manually considered for replay serialization, even when the field is only UI,
settings, debug-window, or app-shell state.

The split should not be treated as a major memory optimization. The heavy replay
payload is still mostly the stage, ent manager, tool state, and other active
simulation data. The primary win is ownership clarity: replay records gameplay
simulation state, not the whole application junk drawer.

## Proposed Shape

```cpp
struct AppState {
    Mode mode;
    Settings settings;
    MenuInputs menu_inputs;
    MenuInputSnapshot menu_input_snapshot;
    MenuInputSnapshot previous_menu_input_snapshot;
    MenuInputDebounceTimers menu_input_debounce_timers;
    menu selection state;
    debug UI/window state;
    performance stats;
    bool running;
    bool rebuild_render_texture;
};

struct PlayState {
    PlayingInputs playing_inputs;
    PlayingInputs immediate_playing_inputs;
    PlayingInputSnapshot playing_input_snapshot;
    PlayingInputSnapshot previous_playing_input_snapshot;
    PlayingInputSnapshot previous_immediate_playing_input_snapshot;

    std::uint32_t frame;
    std::uint32_t stage_frame;
    std::uint32_t frame_pause;

    bool game_over;
    bool pause;
    bool win;

    StageLoadTarget respawn_target;
    std::optional<StageTransitionTarget> pending_stage_transition;
    std::uint32_t points;
    std::uint32_t deaths;
    std::uint32_t depth;
    QuestState quest_state;
    altar/favor state;

    Stage stage;
    StageAcoustics stage_acoustics;
    StageLighting stage_lighting;
    EntPool ents;
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    EntToolInventoryState ent_tools;
    ContactBookkeeping contact;
    SID sid;

    PlayerRegistry players;
    std::optional<VID> controlled_ent_vid;
    std::optional<VID> mouse_trailer_vid;
    prompts and transient per-frame gameplay lists;
};
```

The final names can change. The important boundary is that `PlayState` owns the
simulation. `AppState` owns app shell, menus, persistent settings, debug window
state, and render/app lifecycle toggles.

## Replay Snapshot Target

A future replay snapshot should primarily serialize `PlayState`, plus a small
shell payload for values needed to make playback viewable:

- current `Mode` if replay needs to restore `Playing`, `GameOver`, or `Win`
- playing input snapshots if input determinism/debugging needs them
- gameplay camera position, or a dedicated camera replay state
- maybe a compact debug level id if debugging generated test stages

It should not normally serialize:

- video/settings menu selection state
- target fullscreen/resolution options
- debug window positions or visibility
- persistent settings unrelated to simulation
- performance timing stats
- one-frame debug annotations unless explicitly recording debug data

## Migration Plan

1. Create `PlayState` and move pure simulation fields into it without changing behavior.
2. Keep `State` as the root owner temporarily: `struct State { AppState app; PlayState play; ... }` or `State` plus `play` depending on churn.
3. Add accessor helpers only where they reduce churn; avoid making a global compatibility layer that hides the ownership boundary.
4. Change `GameplaySnapshot` to copy/serialize `PlayState` first.
5. Add only the small app-shell replay fields that are demonstrably required.
6. Delete duplicated snapshot fields once playback is stable.

## Risks

- This is a broad refactor because many systems currently accept `State&` and touch both app and gameplay fields.
- A mechanical split can create worse code if every call site becomes `state.play.foo` without improving ownership.
- Audio, camera, debug tools, and stage transitions straddle the boundary and need deliberate placement.

## Recommendation

Do this after the current Classic Quest stagegen work lands. The stagegen changes
are already touching progression, stage loading, and debug replay. Splitting state
at the same time would increase merge and regression risk without changing the
player-facing result.
