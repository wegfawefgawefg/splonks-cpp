#include "step.hpp"

#include "audio_emitters.hpp"
#include "inputs.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "entities/basic_exit.hpp"
#include "buying.hpp"
#include "step_entities.hpp"
#include "stage_progression.hpp"
#include "stage_rotation.hpp"

namespace splonks {

namespace {

constexpr float kShakeAttenuationRate = 0.1F;
constexpr std::uint32_t kGameOverHitstopFrames = 8;
constexpr std::uint32_t kGameOverSlowmoFrames = 60 * 3;

float GetSimulationTickInterval(const State& state) {
    if (state.mode == Mode::GameOver && state.scene_frame < kGameOverSlowmoFrames) {
        return kTimestep * 2.0F;
    }
    return kTimestep;
}

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

Vec2 GetDefaultGameplayAudioListenerWorldPos(const State& state, const Graphics& graphics) {
    if (state.controlled_entity_vid.has_value()) {
        if (const Entity* const controlled = state.entity_manager.GetEntity(*state.controlled_entity_vid)) {
            return entities::common::GetVisualCenterForEntity(
                *controlled,
                graphics,
                controlled->GetCenter()
            );
        }
    }
    if (state.mode == Mode::GameOver && state.gameplay_camera_anchor_world_pos.has_value()) {
        return *state.gameplay_camera_anchor_world_pos;
    }
    return graphics.camera.target;
}

} // namespace

void Step(State& state, Audio& audio, Graphics& graphics, float frame_dt) {
    state.time_since_last_update += frame_dt;
    while (state.time_since_last_update > GetSimulationTickInterval(state)) {
        state.time_since_last_update -= GetSimulationTickInterval(state);
        StepSingleTick(state, audio, graphics);
    }
}

void StepSingleTick(State& state, Audio& audio, Graphics& graphics) {
    if (IsStageRotationActive(state)) {
        state.ClearDebugAnnotations();
        StepStageRotation(state, graphics);
        state.scene_frame += 1;
        return;
    }

    if (state.frame_pause > 0) {
        state.frame_pause -= 1;
        return;
    }

    state.ClearDebugAnnotations();

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
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::Title as usize]);
}

void StepPlaying(State& state, Audio& audio, Graphics& graphics, float dt) {
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::Playing as usize]);

    UpdateControlledEntity(state);
    LatchPlayingInputsForTick(state);
    if (state.controlled_entity_vid.has_value()) {
        if (Entity* const controlled = state.entity_manager.GetEntityMut(*state.controlled_entity_vid)) {
            if (controlled->active && controlled->control_logic != nullptr) {
                controlled->control_logic(controlled->vid.id, state, graphics, audio, dt);
            }
        }
    }
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjectileBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.entity_tools.Step();
    state.RebuildSid(graphics);
    state.gameplay_camera_anchor_world_pos = graphics.camera.target;
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    state.stage.SyncTileShakeGrid();
    StepEntities(state, audio, graphics, dt);
    UpdateAudioEmitters(state, audio, graphics);
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        AttenuateEntityShake(entity, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    AddBuyPromptsForPlayer(state, graphics);
    if (state.player_vid.has_value() && state.playing_inputs.equip_button.pressed &&
        !state.IsInteractClaimedForEntity(*state.player_vid)) {
        (void)TryBuyOverlappingEntity(state.player_vid->id, state, graphics, audio);
    }
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
        Vec2 game_over_pos = state.gameplay_camera_anchor_world_pos.value_or(graphics.camera.target);
        if (state.player_vid.has_value()) {
            if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
                if (player->active) {
                    game_over_pos = entities::common::GetVisualCenterForEntity(*player, graphics, player->GetCenter());
                    state.controlled_entity_vid = state.player_vid;
                }
            }
        }
        state.gameplay_camera_anchor_world_pos = game_over_pos;
        state.frame_pause += kGameOverHitstopFrames;
        AddShake(state, game_over_pos, 2.2F, 3.0F, ShakeMask::All);
        (void)PlayWorldSoundEmitter(state, game_over_pos, audio_asset_ids::GameOver);
        state.SetMode(Mode::GameOver);
    } else if (state.pending_stage_transition.has_value()) {
        StopAllSoundEmitters(state, audio);
        state.mode = Mode::StageTransition;
        state.frame = 0;
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
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::GameOver as usize]);
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjectileBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.RebuildSid(graphics);
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    StepEntities(state, audio, graphics, dt);
    UpdateAudioEmitters(state, audio, graphics);
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        AttenuateEntityShake(entity, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    state.particles.Step(graphics.frame_data_db, dt);
}

void StepWin(State& state, Audio& audio, Graphics& graphics) {
    (void)state;
    (void)audio;
    (void)graphics;
}

} // namespace splonks
