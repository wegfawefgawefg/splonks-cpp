#include "step.hpp"

#include "audio_emitters.hpp"
#include "inputs.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "entities/basic_exit.hpp"
#include "buying.hpp"
#include "step_entities.hpp"
#include "stage_progression.hpp"
#include "stage_fluids.hpp"
#include "stage_lighting.hpp"
#include "stage_rotation.hpp"
#include "network/net_lobby.hpp"
#include "network/net_progression.hpp"
#include "player_queries.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace splonks {

namespace {

constexpr float kShakeAttenuationRate = 0.1F;
constexpr std::uint32_t kGameOverHitstopFrames = 8;
constexpr std::uint32_t kGameOverSlowmoFrames = 60 * 3;
constexpr float kPlayerLampLightStrength = 1.45F;
constexpr int kPlayerLampLightRadius = 13;
constexpr std::uint32_t kNetworkStageTransitionFrames = 60;

std::uint32_t MakeNetworkTransitionSeed(const State& state) {
    return (state.frame + 1U) ^ (state.stage_frame << 7U) ^ 0x6D2B79F5U;
}

void ApplyPendingStageTransitionNow(State& state, Graphics& graphics) {
    if (state.net_session.role != network::NetRole::Offline &&
        state.pending_stage_transition.has_value() &&
        state.pending_stage_transition->destination.kind == StageLoadTargetKind::QuestStage &&
        !state.pending_stage_transition->seed.has_value()) {
        state.pending_stage_transition->seed = MakeNetworkTransitionSeed(state);
    }

    ApplyPendingStageTransition(state);
    graphics.ResetTileVariations();
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
    if (state.net_session.role != network::NetRole::Offline) {
        std::string status;
        (void)network::ReviveNetworkPlayersAtEntrance(state, graphics, &status);
    }
    network::NotifyStageLoaded(state);
}

float GetSimulationTickInterval(const State& state) {
    if (state.mode == Mode::GameOver && state.scene_frame < kGameOverSlowmoFrames) {
        return kTimestep * 2.0F;
    }
    return kTimestep;
}

const Entity* GetLivingPlayerForSlot(const State& state, const PlayerSlot& slot) {
    if (!slot.connected || !slot.entity_vid.has_value()) {
        return nullptr;
    }
    const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return nullptr;
    }
    return player;
}

std::optional<VID> FindLivingPlayerVidByPlayerId(const State& state, PlayerId player_id) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    const Entity* const player = slot != nullptr ? GetLivingPlayerForSlot(state, *slot) : nullptr;
    return player != nullptr ? std::optional<VID>(player->vid) : std::nullopt;
}

std::vector<PlayerId> GetLivingConnectedPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.player_id != kInvalidPlayerId && GetLivingPlayerForSlot(state, slot) != nullptr) {
            player_ids.push_back(slot.player_id);
        }
    }
    return player_ids;
}

std::optional<PlayerId> CycleLivingPlayerId(
    const State& state,
    std::optional<PlayerId> current,
    int direction
) {
    const std::vector<PlayerId> living_player_ids = GetLivingConnectedPlayerIds(state);
    if (living_player_ids.empty()) {
        return std::nullopt;
    }
    if (direction == 0 || !current.has_value()) {
        return living_player_ids.front();
    }

    const auto iter = std::find(living_player_ids.begin(), living_player_ids.end(), *current);
    const int count = static_cast<int>(living_player_ids.size());
    int index = iter != living_player_ids.end()
        ? static_cast<int>(std::distance(living_player_ids.begin(), iter))
        : 0;
    index = (index + direction) % count;
    if (index < 0) {
        index += count;
    }
    return living_player_ids[static_cast<std::size_t>(index)];
}

void UpdateSpectatorTarget(State& state) {
    if (state.net_session.role == network::NetRole::Offline) {
        state.spectator_target_player_id.reset();
        return;
    }

    if (const std::optional<VID> primary_player_vid = FindPrimaryLocalPlayerVid(state)) {
        const Entity* const player = state.entity_manager.GetEntity(*primary_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntityCondition::Dead) {
            state.spectator_target_player_id.reset();
            return;
        }
    }

    int cycle_dir = 0;
    if (state.playing_inputs.right.pressed && !state.playing_inputs.left.pressed) {
        cycle_dir = 1;
    } else if (state.playing_inputs.left.pressed && !state.playing_inputs.right.pressed) {
        cycle_dir = -1;
    }
    state.spectator_target_player_id =
        CycleLivingPlayerId(state, state.spectator_target_player_id, cycle_dir);
}

std::optional<VID> FindCameraControlledPlayerVid(const State& state) {
    if (state.spectator_target_player_id.has_value()) {
        if (const std::optional<VID> spectator_vid =
                FindLivingPlayerVidByPlayerId(state, *state.spectator_target_player_id)) {
            return spectator_vid;
        }
    }

    if (const std::optional<VID> primary_player_vid = FindPrimaryLocalPlayerVid(state)) {
        const Entity* const player = state.entity_manager.GetEntity(*primary_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntityCondition::Dead) {
            return primary_player_vid;
        }
    }
    return FindFirstConnectedLivingPlayerVid(state);
}

void UpdateControlledEntity(State& state) {
    if (!state.controlled_entity_vid.has_value()) {
        state.controlled_entity_vid = FindCameraControlledPlayerVid(state);
        return;
    }

    const Entity* controlled = state.entity_manager.GetEntity(*state.controlled_entity_vid);
    const bool invalid_controlled =
        controlled == nullptr || !controlled->active ||
        controlled->condition == EntityCondition::Dead;
    if (!invalid_controlled) {
        return;
    }

    if (const std::optional<VID> camera_player_vid = FindCameraControlledPlayerVid(state)) {
        const Entity* player = state.entity_manager.GetEntity(*camera_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntityCondition::Dead) {
            state.controlled_entity_vid = camera_player_vid;
            return;
        }
    }

    state.controlled_entity_vid.reset();
}

void RefreshPlayableCharacterLamp(State& state) {
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active || !IsPlayerLikeEntityType(entity.type_)) {
            continue;
        }

        bool emits_lamp =
            state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
        if (state.net_session.input_lockstep_enabled) {
            const PlayerSlot* const slot = state.players.FindByEntityVid(entity.vid);
            emits_lamp = slot != nullptr && slot->connected;
        }

        if (!emits_lamp || entity.condition == EntityCondition::Dead) {
            entity.light_strength = 0.0F;
            entity.light_radius = 0;
            entity.light_color = Color3::White();
            continue;
        }

        entity.light_strength = kPlayerLampLightStrength *
                                std::max(state.settings.post_process.player_lamp_strength, 0.0F);
        entity.light_radius = kPlayerLampLightRadius;
        entity.light_color = Color3::White();
    }
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

bool ShouldEnterGameOver(const State& state, std::optional<VID> primary_player_vid) {
    if (state.net_session.role != network::NetRole::Offline ||
        state.net_session.input_lockstep_enabled) {
        return HasAnyConnectedPlayerSlot(state) && !HasAnyConnectedLivingPlayer(state);
    }

    if (!primary_player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*primary_player_vid);
    return player == nullptr || !player->active || player->condition == EntityCondition::Dead;
}

void StepPlayerSlotControls(State& state, Graphics& graphics, Audio& audio, float dt) {
    bool stepped_any_player_slot = false;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
        if (entity == nullptr || !entity->active || entity->control_logic == nullptr) {
            continue;
        }

        entity->control_logic(entity->vid.id, state, graphics, audio, dt);
        stepped_any_player_slot = true;
    }

    if (stepped_any_player_slot) {
        return;
    }

    if (state.controlled_entity_vid.has_value()) {
        if (Entity* const controlled = state.entity_manager.GetEntityMut(*state.controlled_entity_vid)) {
            if (controlled->active && controlled->control_logic != nullptr) {
                controlled->control_logic(controlled->vid.id, state, graphics, audio, dt);
            }
        }
    }
}

ButtonState MakeDebugBotButtonState(bool down, bool previous_down) {
    return ButtonState{
        .down = down,
        .pressed = down && !previous_down,
        .released = !down && previous_down,
    };
}

PlayingInputs MakeDebugBotInputs(DebugLocalPlayerBot& bot) {
    if (bot.retarget_frames <= 0) {
        bot.move_dir = rng::RandomIntInclusive(-1, 1);
        bot.retarget_frames = rng::RandomIntInclusive(24, 90);
    } else {
        bot.retarget_frames -= 1;
    }

    bool jump_down = false;
    if (bot.jump_cooldown_frames > 0) {
        bot.jump_cooldown_frames -= 1;
    } else if (bot.allow_jump && rng::RandomIntInclusive(1, 45) == 1) {
        jump_down = true;
        bot.jump_cooldown_frames = rng::RandomIntInclusive(20, 70);
    }

    const bool left_down = bot.enabled && bot.move_dir < 0;
    const bool right_down = bot.enabled && bot.move_dir > 0;
    const bool run_down = bot.enabled && rng::RandomIntInclusive(1, 5) == 1;

    PlayingInputs inputs = PlayingInputs::New();
    inputs.left = MakeDebugBotButtonState(left_down, bot.previous_inputs.left.down);
    inputs.right = MakeDebugBotButtonState(right_down, bot.previous_inputs.right.down);
    inputs.jump = MakeDebugBotButtonState(bot.enabled && jump_down, bot.previous_inputs.jump.down);
    inputs.run = MakeDebugBotButtonState(run_down, bot.previous_inputs.run.down);
    if (bot.allow_tools) {
        const bool attack_down = bot.enabled && rng::RandomIntInclusive(1, 180) == 1;
        const bool bomb_down = bot.enabled && rng::RandomIntInclusive(1, 420) == 1;
        const bool rope_down = bot.enabled && rng::RandomIntInclusive(1, 420) == 1;
        inputs.attack = MakeDebugBotButtonState(attack_down, bot.previous_inputs.attack.down);
        inputs.bomb = MakeDebugBotButtonState(bomb_down, bot.previous_inputs.bomb.down);
        inputs.rope = MakeDebugBotButtonState(rope_down, bot.previous_inputs.rope.down);
    }
    inputs.mouse_pos = bot.previous_inputs.mouse_pos;
    bot.previous_inputs = inputs;
    return inputs;
}

void StepDebugLocalPlayerBots(State& state) {
    for (DebugLocalPlayerBot& bot : state.debug_local_player_bots) {
        if (!state.players.Find(bot.player_id)) {
            continue;
        }
        PlayingInputs inputs = bot.enabled ? MakeDebugBotInputs(bot) : PlayingInputs::New();
        state.players.SetInputsForPlayer(bot.player_id, inputs, inputs);
    }
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
    if (state.mode == Mode::Playing &&
        network::IsInputLockstepActive(state) &&
        !network::PrepareInputLockstepFrame(state, graphics)) {
        return;
    }

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

    StepTransientLights(state);
    LatchPlayingInputsForTick(state);
    UpdateSpectatorTarget(state);
    UpdateControlledEntity(state);
    RefreshPlayableCharacterLamp(state);
    StepDebugLocalPlayerBots(state);
    StepPlayerSlotControls(state, graphics, audio, dt);
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjectileBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.entity_tools.Step();
    StepStageFluids(state);
    state.RebuildSid(graphics);
    state.gameplay_camera_anchor_world_pos = graphics.camera.target;
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    state.stage.SyncTileShakeGrid();
    StepEntities(state, audio, graphics, dt);
    network::StepNetworkLobby(state, graphics);
    UpdateAudioEmitters(state, audio, graphics);
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        AttenuateEntityShake(entity, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    AddBuyPromptsForPlayer(state, graphics);
    const std::optional<VID> primary_player_vid = FindPrimaryLocalPlayerVid(state);
    if (primary_player_vid.has_value() && state.playing_inputs.equip_button.pressed &&
        !state.IsInteractClaimedForEntity(*primary_player_vid)) {
        const std::optional<std::size_t> buyable_idx =
            FindOverlappingBuyableEntityIdx(state, graphics, primary_player_vid->id);
        if (buyable_idx.has_value()) {
            (void)world_ops::TryApplyInteractEntity(
                *primary_player_vid,
                state.entity_manager.entities[*buyable_idx].vid,
                state,
                graphics,
                audio
            );
        }
    }
    state.particles.Step(graphics.frame_data_db, dt);

    const bool lost = ShouldEnterGameOver(state, primary_player_vid);
    if (state.pending_stage_transition.has_value()) {
        StopAllSoundEmitters(state, audio);
        state.SetMode(Mode::StageTransition);
        state.frame = 0;
    } else if (lost) {
        Vec2 game_over_pos = state.gameplay_camera_anchor_world_pos.value_or(graphics.camera.target);
        if (primary_player_vid.has_value()) {
            if (const Entity* const player = state.entity_manager.GetEntity(*primary_player_vid)) {
                if (player->active) {
                    game_over_pos = entities::common::GetVisualCenterForEntity(*player, graphics, player->GetCenter());
                    state.controlled_entity_vid = primary_player_vid;
                }
            }
        }
        state.gameplay_camera_anchor_world_pos = game_over_pos;
        state.frame_pause += kGameOverHitstopFrames;
        AddShake(state, game_over_pos, 2.2F, 3.0F, ShakeMask::All);
        (void)PlayWorldSoundEmitter(state, game_over_pos, audio_asset_ids::GameOver);
        state.game_over = true;
        state.SetMode(Mode::GameOver);
    }

    // step_camera(rl, rlt, state, graphics);
    //TODO: STAGE NEEDS STEPPING
    // self.stage.update();
    state.frame += 1;
    state.stage_frame += 1;
}

void StepStageTransition(State& state, Audio& audio, Graphics& graphics) {
    (void)audio;
    network::StepNetworkLobby(state, graphics);

    if (network::IsInputLockstepSession(state)) {
        if (state.scene_frame < kNetworkStageTransitionFrames) {
            return;
        }
        if (!state.pending_stage_transition.has_value()) {
            state.SetMode(Mode::Win);
            return;
        }

        ApplyPendingStageTransitionNow(state, graphics);
        state.scene_frame = 0;
        state.SetMode(Mode::Playing);
        return;
    }

    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }
    if (!state.pending_stage_transition.has_value()) {
        state.SetMode(Mode::Win);
        return;
    }
    if (state.scene_frame < kNetworkStageTransitionFrames) {
        return;
    }

    ApplyPendingStageTransitionNow(state, graphics);
    state.scene_frame = 0;
    state.SetMode(Mode::Playing);
}

void StepGameOver(State& state, Audio& audio, Graphics& graphics, float dt) {
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::GameOver as usize]);
    StepTransientLights(state);
    LatchPlayingInputsForTick(state);
    UpdateSpectatorTarget(state);
    UpdateControlledEntity(state);
    RefreshPlayableCharacterLamp(state);
    StepDebugLocalPlayerBots(state);
    StepPlayerSlotControls(state, graphics, audio, dt);
    state.contact.ClearEntityContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjectileBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.entity_tools.Step();
    StepStageFluids(state);
    state.RebuildSid(graphics);
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    state.stage.SyncTileShakeGrid();
    StepEntities(state, audio, graphics, dt);
    network::StepNetworkLobby(state, graphics);
    UpdateAudioEmitters(state, audio, graphics);
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        AttenuateEntityShake(entity, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    state.particles.Step(graphics.frame_data_db, dt);
    state.frame += 1;
    state.stage_frame += 1;
    state.scene_frame += 1;
}

void StepWin(State& state, Audio& audio, Graphics& graphics) {
    (void)state;
    (void)audio;
    (void)graphics;
}

} // namespace splonks
