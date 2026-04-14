#include "step.hpp"

#include "inputs.hpp"
#include "controls.hpp"
#include "step_entities.hpp"
#include "stage_progression.hpp"

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
        controlled->condition == EntityCondition::Dead;
    if (!invalid_controlled) {
        return;
    }

    if (state.player_vid.has_value()) {
        const Entity* player = state.entity_manager.GetEntity(*state.player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntityCondition::Dead) {
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
        controls::ControlEntityAsPlayer(*state.player_vid, state);
    }
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.entity_tools.Step();
    state.RebuildSid(graphics);
    StepEntities(state, audio, graphics, dt);
    state.particles.Step(graphics.frame_data_db, dt);

    bool lost = false;
    if (state.player_vid) {
        if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
            if (player->condition == EntityCondition::Dead) {
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
        state.pending_stage_transition.reset();
        audio.PlaySoundEffect(SoundEffect::GameOver);
        state.SetMode(Mode::GameOver);
    } else if (state.pending_stage_transition.has_value()) {
        state.mode = Mode::StageTransition;
        state.frame = 0;
    } else if (IsStageWon(state)) {
        // TODO: make this go into a stage transition tree, instead of looping to the begining lol
        audio.PlaySoundEffect(SoundEffect::StageWin);

        switch (state.stage.stage_type) {
        case StageType::Test1:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Test1), true);
            break;
        case StageType::SplkMines1:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::SplkMines2), true);
            break;
        case StageType::SplkMines2:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::SplkMines3), true);
            break;
        case StageType::SplkMines3:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice1), true);
            break;
        case StageType::Ice1:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice2), true);
            break;
        case StageType::Ice2:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Ice3), true);
            break;
        case StageType::Ice3:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert1), true);
            break;
        case StageType::Desert1:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert2), true);
            break;
        case StageType::Desert2:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Desert3), true);
            break;
        case StageType::Desert3:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple1), true);
            break;
        case StageType::Temple1:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple2), true);
            break;
        case StageType::Temple2:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Temple3), true);
            break;
        case StageType::Temple3:
            QueueStageTransition(state, StageLoadTarget::ForStageType(StageType::Boss), true);
            break;
        case StageType::Boss:
            state.stage = Stage::NewBlank();
            state.mode = Mode::Win;
            break;
        case StageType::Blank:
            state.stage = Stage::NewBlank();
            state.mode = Mode::Win;
            break;
        }
        if (state.pending_stage_transition.has_value()) {
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
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.RebuildSid(graphics);
    StepEntities(state, audio, graphics, dt);
    state.particles.Step(graphics.frame_data_db, dt);
}

void StepWin(State& state, Audio& audio, Graphics& graphics) {
    (void)state;
    (void)audio;
    (void)graphics;
}

} // namespace splonks
