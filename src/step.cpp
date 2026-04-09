#include "step.hpp"

#include "inputs.hpp"
#include "systems/controls.hpp"
#include "step_entities.hpp"

namespace splonks {

namespace {

void UpdateControlledEntity(State& state) {
    if (!state.controlled_entity_vid.has_value()) {
        state.controlled_entity_vid = state.player_vid;
        return;
    }

    const Entity* controlled = state.entity_manager.GetEntity(*state.controlled_entity_vid);
    const bool invalid_controlled =
        controlled == nullptr || !controlled->active ||
        controlled->super_state == EntitySuperState::Dead ||
        controlled->super_state == EntitySuperState::Crushed;
    if (!invalid_controlled) {
        return;
    }

    if (state.player_vid.has_value()) {
        const Entity* player = state.entity_manager.GetEntity(*state.player_vid);
        if (player != nullptr && player->active &&
            player->super_state != EntitySuperState::Dead &&
            player->super_state != EntitySuperState::Crushed) {
            state.controlled_entity_vid = state.player_vid;
            return;
        }
    }

    state.controlled_entity_vid.reset();
}

} // namespace

void Step(State& state, Audio& audio, Graphics& graphics, float frame_dt) {
    state.time_since_last_update += frame_dt;
    while (state.time_since_last_update > kTimestep) {
        state.time_since_last_update -= kTimestep;
        StepSingleTick(state, audio, graphics);
    }
}

void StepSingleTick(State& state, Audio& audio, Graphics& graphics) {
    if (state.frame_pause > 0) {
        state.frame_pause -= 1;
        return;
    }

    switch (state.mode) {
    case Mode::Title:
        StepTitle(state, audio);
        break;
    case Mode::Settings:
        break;
    case Mode::VideoSettings:
        break;
    case Mode::UiSettings:
    case Mode::PostFxSettings:
    case Mode::LightingSettings:
        break;
    case Mode::Playing:
        StepPlaying(state, audio, graphics, kTimestep);
        break;
    case Mode::StageTransition:
        StepStageTransition(state, audio, graphics);
        break;
    case Mode::GameOver:
        StepGameOver(state, audio, graphics, kTimestep);
        break;
    case Mode::Win:
        StepWin(state, audio, graphics);
        break;
    }
    state.scene_frame += 1;
}

void StepTitle(State& state, Audio& audio) {
    (void)state;
    (void)audio;
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[Song::Title as usize]);
}

void StepPlaying(State& state, Audio& audio, Graphics& graphics, float dt) {
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[Song::Playing as usize]);

    UpdateControlledEntity(state);
    LatchPlayingInputsForTick(state);
    if (state.player_vid.has_value()) {
        systems::controls::ControlEntityAsPlayer(*state.player_vid, state);
    }
    state.StepContactCooldowns();
    state.StepEntityToolStates();
    state.RebuildSid();
    StepEntities(state, audio, graphics, dt);
    StepSpecialEffects(state);

    bool lost = false;
    if (state.player_vid) {
        if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
            if (player->super_state == EntitySuperState::Dead) {
                lost = true;
            } else {
                lost = false;
            }
        } else {
            lost = true;
        }
    } else {
        lost = false;
    }
    if (lost) {
        audio.PlaySoundEffect(SoundEffect::GameOver);
        state.SetMode(Mode::GameOver);
    } else if (IsStageWon(state)) {
        // TODO: make this go into a stage transition tree, instead of looping to the begining lol
        audio.PlaySoundEffect(SoundEffect::StageWin);

        switch (state.stage.stage_type) {
        case StageType::Test1:
            state.next_stage = StageType::Test1;
            break;
        case StageType::Cave1:
            state.next_stage = StageType::Cave2;
            break;
        case StageType::Cave2:
            state.next_stage = StageType::Cave3;
            break;
        case StageType::Cave3:
            state.next_stage = StageType::Ice1;
            break;
        case StageType::Ice1:
            state.next_stage = StageType::Ice2;
            break;
        case StageType::Ice2:
            state.next_stage = StageType::Ice3;
            break;
        case StageType::Ice3:
            state.next_stage = StageType::Desert1;
            break;
        case StageType::Desert1:
            state.next_stage = StageType::Desert2;
            break;
        case StageType::Desert2:
            state.next_stage = StageType::Desert3;
            break;
        case StageType::Desert3:
            state.next_stage = StageType::Temple1;
            break;
        case StageType::Temple1:
            state.next_stage = StageType::Temple2;
            break;
        case StageType::Temple2:
            state.next_stage = StageType::Temple3;
            break;
        case StageType::Temple3:
            state.next_stage = StageType::Boss;
            break;
        case StageType::Boss:
            state.next_stage.reset();
            break;
        case StageType::Blank:
            state.next_stage.reset();
            break;
        }
        if (!state.next_stage) {
            state.stage = Stage::NewBlank();
            state.mode = Mode::Win;
        } else {
            state.mode = Mode::StageTransition;
            state.frame = 0;
        }
    }

    // step_camera(rl, rlt, state, graphics);
    //TODO: STAGE NEEDS STEPPING
    // self.stage.update();
    state.frame += 1;
    state.stage_frame += 1;
}

void StepStageTransition(State& state, Audio& audio, Graphics& graphics) {
    (void)state;
    (void)audio;
    (void)graphics;
    // figure out what stage comes next,
    // set up the game state.
    // fall the player in.
}

void StepGameOver(State& state, Audio& audio, Graphics& graphics, float dt) {
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[Song::GameOver as usize]);
    state.StepContactCooldowns();
    state.RebuildSid();
    StepEntities(state, audio, graphics, dt);
    StepSpecialEffects(state);
}

void StepWin(State& state, Audio& audio, Graphics& graphics) {
    (void)state;
    (void)audio;
    (void)graphics;
}

} // namespace splonks
