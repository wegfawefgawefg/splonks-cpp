#include "step.hpp"

#include "audio_emitters.hpp"
#include "debug/input_bot.hpp"
#include "inputs.hpp"
#include "controls.hpp"
#include "ents/common/common.hpp"
#include "ents/basic_exit.hpp"
#include "buying.hpp"
#include "step_ents.hpp"
#include "stage_progression.hpp"
#include "stage_fluids.hpp"
#include "stage_lighting.hpp"
#include "stage_rotation.hpp"
#include "network/net_lobby.hpp"
#include "network/net_progression.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace splonks {

namespace {

constexpr float kShakeAttenuationRate = 0.1F;
constexpr std::uint32_t kGameOverHitstopFrames = 8;
constexpr std::uint32_t kGameOverSlowmoFrames = 60 * 3;
constexpr float kPlayerLampLightStrength = 1.45F;
constexpr int kPlayerLampLightRadius = 13;
constexpr std::uint32_t kNetworkStageTransitionFrames = 60;

template <std::size_t N>
std::string_view FixedStringView(const std::array<char, N>& value) {
    const char* const begin = value.data();
    const char* const end = std::find(begin, begin + value.size(), '\0');
    return std::string_view(begin, static_cast<std::size_t>(end - begin));
}

std::uint32_t MixTransitionSeed(std::uint32_t seed, std::uint32_t value) {
    seed ^= value + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

std::uint32_t MixTransitionSeed(std::uint32_t seed, std::string_view value) {
    for (const char c : value) {
        seed = MixTransitionSeed(seed, static_cast<std::uint8_t>(c));
    }
    return seed;
}

std::uint32_t MakeNetworkTransitionSeed(const State& state, const StageTransitionTarget& target) {
    std::uint32_t seed = state.stage.generation_seed.value_or(0x51A7E5D3U);
    seed = MixTransitionSeed(seed, state.depth);
    seed = MixTransitionSeed(seed, static_cast<std::uint8_t>(target.destination.kind));
    seed = MixTransitionSeed(seed, static_cast<std::uint8_t>(target.destination.stage_type));
    seed = MixTransitionSeed(seed, static_cast<std::uint8_t>(target.destination.debug_level));
    seed = MixTransitionSeed(seed, target.destination.debug_variant);
    seed = MixTransitionSeed(seed, FixedStringView(target.destination.quest_id));
    seed = MixTransitionSeed(seed, FixedStringView(target.destination.quest_stage_id));
    return seed == 0 ? 1U : seed;
}

void ApplyNetworkRespawnPolicyAfterStageLoad(State& state, Graphics& graphics) {
    if (state.net_session.role == network::NetRole::Offline) {
        return;
    }

    std::string status;
    switch (state.multiplayer_respawn_mode) {
    case MultiplayerRespawnMode::GenerousNextLevel:
        (void)network::ReviveNetworkPlayersAtEntrance(state, graphics, &status);
        break;
    case MultiplayerRespawnMode::NoRespawn:
        state.game_over = false;
        state.gameplay_camera_anchor_world_pos.reset();
        break;
    case MultiplayerRespawnMode::RespawnAtEntrance:
        (void)network::RespawnDeadNetworkPlayersAtEntrance(state, graphics, &status);
        state.game_over = false;
        state.gameplay_camera_anchor_world_pos.reset();
        break;
    }
}

void ApplyPendingStageTransitionNow(State& state, Graphics& graphics) {
    if (state.net_session.role != network::NetRole::Offline &&
        state.pending_stage_transition.has_value() &&
        state.pending_stage_transition->destination.kind == StageLoadTargetKind::QuestStage &&
        !state.pending_stage_transition->seed.has_value()) {
        state.pending_stage_transition->seed =
            MakeNetworkTransitionSeed(state, *state.pending_stage_transition);
    }

    bool applied_network_fresh_quest_reload = false;
    if (state.net_session.role != network::NetRole::Offline &&
        state.pending_stage_transition.has_value() &&
        state.pending_stage_transition->destination.kind == StageLoadTargetKind::QuestStage &&
        !state.pending_stage_transition->preserve_player_state) {
        const StageTransitionTarget target = *state.pending_stage_transition;
        state.pending_stage_transition.reset();
        state.net_session.quest_id = std::string(FixedStringView(target.destination.quest_id));
        state.net_session.quest_stage_id =
            std::string(FixedStringView(target.destination.quest_stage_id));
        state.net_session.stage_seed = target.seed.value_or(MakeNetworkTransitionSeed(state, target));
        applied_network_fresh_quest_reload =
            network::ReloadSyncedQuestStage(state, graphics, nullptr);
        if (!applied_network_fresh_quest_reload) {
            state.pending_stage_transition = target;
        }
    }
    if (!applied_network_fresh_quest_reload) {
        ApplyPendingStageTransition(state);
    }
    graphics.ResetTileVariations();
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
    ApplyNetworkRespawnPolicyAfterStageLoad(state, graphics);
    network::NotifyStageLoaded(state);
}

float GetSimulationTickInterval(const State& state) {
    if (state.mode == Mode::GameOver && state.scene_frame < kGameOverSlowmoFrames) {
        return kTimestep * 2.0F;
    }
    return kTimestep;
}

const Ent* GetLivingPlayerForSlot(const State& state, const PlayerSlot& slot) {
    if (!slot.connected || !slot.ent_vid.has_value()) {
        return nullptr;
    }
    const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
    if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
        return nullptr;
    }
    return player;
}

std::optional<VID> FindLivingPlayerVidByPlayerId(const State& state, PlayerId player_id) {
    const PlayerSlot* const slot = state.players.Find(player_id);
    const Ent* const player = slot != nullptr ? GetLivingPlayerForSlot(state, *slot) : nullptr;
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
        const Ent* const player = state.ents.GetEnt(*primary_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntCondition::Dead) {
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
        const Ent* const player = state.ents.GetEnt(*primary_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntCondition::Dead) {
            return primary_player_vid;
        }
    }
    return FindFirstConnectedLivingPlayerVid(state);
}

void UpdateControlledEnt(State& state) {
    if (!state.controlled_ent_vid.has_value()) {
        state.controlled_ent_vid = FindCameraControlledPlayerVid(state);
        return;
    }

    const Ent* controlled = state.ents.GetEnt(*state.controlled_ent_vid);
    const bool invalid_controlled =
        controlled == nullptr || !controlled->active ||
        controlled->condition == EntCondition::Dead;
    if (!invalid_controlled) {
        return;
    }

    if (const std::optional<VID> camera_player_vid = FindCameraControlledPlayerVid(state)) {
        const Ent* player = state.ents.GetEnt(*camera_player_vid);
        if (player != nullptr && player->active &&
            player->condition != EntCondition::Dead) {
            state.controlled_ent_vid = camera_player_vid;
            return;
        }
    }

    state.controlled_ent_vid.reset();
}

bool AnyConnectedPlayerConfirmDown(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.player_id == kInvalidPlayerId || !slot.connected) {
            continue;
        }
        if (slot.inputs.jump.down) {
            return true;
        }
    }
    return false;
}

void ApplyLockstepGameOverConfirm(State& state, Graphics& graphics) {
    if (!network::IsInputLockstepSession(state) || state.scene_frame < 60 ||
        !AnyConnectedPlayerConfirmDown(state)) {
        return;
    }

    if (HasAnyConnectedLivingPlayer(state)) {
        state.game_over = false;
        state.scene_frame = 0;
        state.SetMode(Mode::Playing);
        return;
    }

    std::string status;
    if (network::RespawnLocalPlayersAtEntrance(state, graphics, &status)) {
        graphics.camera.rotation = 0.0F;
        InvalidateStageLighting(state);
        InvalidateStageAcoustics(state);
        state.scene_frame = 0;
        state.SetMode(Mode::Playing);
        return;
    }

    QueueRespawnTransition(state);
    graphics.camera.rotation = 0.0F;
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
    state.SetMode(Mode::StageTransition);
}

void RefreshPlayableCharacterLamp(State& state) {
    for (Ent& ent : state.ents.ents) {
        if (!ent.active || !IsPlayerLikeEntType(ent.type_)) {
            continue;
        }

        const PlayerSlot* const slot = state.players.FindByEntVid(ent.vid);
        const bool emits_lamp = slot != nullptr && slot->connected;

        if (!emits_lamp || ent.condition == EntCondition::Dead) {
            ent.light_strength = sim::Scalar::zero();
            ent.light_radius = 0;
            ent.light_color = sim::ToSimColor3(Color3::White());
            continue;
        }

        ent.light_strength = sim::ToSimScalar(
            kPlayerLampLightStrength *
            std::max(state.settings.post_process.player_lamp_strength, 0.0F)
        );
        ent.light_radius = kPlayerLampLightRadius;
        ent.light_color = sim::ToSimColor3(Color3::White());
    }
}

Vec2 GetDefaultGameplayAudioListenerWorldPos(const State& state, const Graphics& graphics) {
    if (state.controlled_ent_vid.has_value()) {
        if (const Ent* const controlled = state.ents.GetEnt(*state.controlled_ent_vid)) {
            return ents::common::GetVisualCenterForEnt(
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

    const Ent* const player = state.ents.GetEnt(*primary_player_vid);
    return player == nullptr || !player->active || player->condition == EntCondition::Dead;
}

void StepPlayerSlotControls(State& state, Graphics& graphics, Audio& audio, float dt) {
    bool stepped_any_player_slot = false;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        Ent* const ent = state.ents.GetEntMut(*slot.ent_vid);
        if (ent == nullptr || !ent->active || ent->control_logic == nullptr) {
            continue;
        }

        ent->control_logic(ent->vid.id, state, graphics, audio, dt);
        stepped_any_player_slot = true;
    }

    if (stepped_any_player_slot) {
        return;
    }

    if (state.controlled_ent_vid.has_value()) {
        if (Ent* const controlled = state.ents.GetEntMut(*state.controlled_ent_vid)) {
            if (controlled->active && controlled->control_logic != nullptr) {
                controlled->control_logic(controlled->vid.id, state, graphics, audio, dt);
            }
        }
    }
}

void ApplyBuyInputsForPlayerSlots(State& state, Graphics& graphics, Audio& audio) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot) ||
            !slot.inputs.equip_button.pressed ||
            state.IsInteractClaimedForEnt(*slot.ent_vid)) {
            continue;
        }

        const std::optional<std::size_t> buyable_idx =
            FindOverlappingBuyableEntIdx(state, graphics, slot.ent_vid->id);
        if (!buyable_idx.has_value()) {
            continue;
        }

        (void)world_ops::TryApplyInteractEnt(
            *slot.ent_vid,
            state.ents.ents[*buyable_idx].vid,
            state,
            graphics,
            audio
        );
    }
}

void StepDebugLocalPlayerBots(State& state) {
    for (DebugLocalPlayerBot& bot : state.debug_local_player_bots) {
        if (!state.players.Find(bot.player_id)) {
            continue;
        }
        PlayingInputs inputs = bot.enabled ? debug::MakeDebugBotInputs(bot) : PlayingInputs::New();
        state.players.SetInputsForPlayer(bot.player_id, inputs, inputs);
    }
}

bool ModeAdvancesLockstepSimulation(Mode mode) {
    return mode == Mode::Playing || mode == Mode::GameOver;
}

void MaintainNetworkForFixedTick(
    State& state,
    Graphics& graphics,
    SimulationTickMode mode
) {
    if (mode != SimulationTickMode::Normal) {
        return;
    }

    if (network::IsInputLockstepActive(state)) {
        network::MaintainInputLockstepTransport(state, graphics);
        return;
    }

    network::StepNetworkLobby(state, graphics);
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
    StepSingleTickWithMode(state, audio, graphics, SimulationTickMode::Normal);
}

void StepSingleTickWithMode(
    State& state,
    Audio& audio,
    Graphics& graphics,
    SimulationTickMode mode
) {
    MaintainNetworkForFixedTick(state, graphics, mode);

    if (mode == SimulationTickMode::Normal &&
        ModeAdvancesLockstepSimulation(state.mode) &&
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
        StepPlaying(state, audio, graphics, kTimestep, mode);
        break;
    case Mode::StageTransition:
        StepStageTransition(state, audio, graphics, mode);
        break;
    case Mode::GameOver:
        StepGameOver(state, audio, graphics, kTimestep, mode);
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
    StepPlaying(state, audio, graphics, dt, SimulationTickMode::Normal);
}

void StepPlaying(
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt,
    SimulationTickMode mode
) {
    (void)mode;
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::Playing as usize]);

    StepTransientLights(state);
    LatchPlayingInputsForTick(state);
    UpdateSpectatorTarget(state);
    UpdateControlledEnt(state);
    RefreshPlayableCharacterLamp(state);
    StepDebugLocalPlayerBots(state);
    StepPlayerSlotControls(state, graphics, audio, dt);
    state.contact.ClearEntContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.ent_tools.Step();
    StepStageFluids(state);
    state.RebuildSid(graphics);
    state.gameplay_camera_anchor_world_pos = graphics.camera.target;
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    state.stage.SyncTileShakeGrid();
    StepEnts(state, audio, graphics, dt);
    UpdateAudioEmitters(state, audio, graphics);
    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        AttenuateEntShake(ent, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    AddBuyPromptsForPlayer(state, graphics);
    const std::optional<VID> primary_player_vid = FindPrimaryLocalPlayerVid(state);
    ApplyBuyInputsForPlayerSlots(state, graphics, audio);
    state.particles.Step(graphics.aframe_db, dt);

    if (state.net_session.role != network::NetRole::Offline &&
        state.multiplayer_respawn_mode == MultiplayerRespawnMode::RespawnAtEntrance) {
        std::string status;
        (void)network::RespawnDeadNetworkPlayersAtEntrance(state, graphics, &status);
    }

    const bool lost = ShouldEnterGameOver(state, primary_player_vid);
    if (state.pending_stage_transition.has_value()) {
        StopAllSoundEmitters(state, audio);
        state.SetMode(Mode::StageTransition);
        state.frame = 0;
    } else if (lost) {
        Vec2 game_over_pos = state.gameplay_camera_anchor_world_pos.value_or(graphics.camera.target);
        if (primary_player_vid.has_value()) {
            if (const Ent* const player = state.ents.GetEnt(*primary_player_vid)) {
                if (player->active) {
                    game_over_pos = ents::common::GetVisualCenterForEnt(*player, graphics, player->GetCenter());
                    state.controlled_ent_vid = primary_player_vid;
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

void StepStageTransition(
    State& state,
    Audio& audio,
    Graphics& graphics,
    SimulationTickMode mode
) {
    (void)audio;

    if (network::IsInputLockstepSession(state)) {
        if (state.scene_frame < kNetworkStageTransitionFrames) {
            return;
        }
        if (state.pending_stage_transition.has_value() &&
            !state.pending_stage_transition->preserve_player_state) {
            ApplyPendingStageTransitionNow(state, graphics);
            state.scene_frame = 0;
            state.SetMode(Mode::Playing);
            return;
        }
        if (mode == SimulationTickMode::Normal &&
            network::IsInputLockstepCatchupBlocking(state)) {
            return;
        }
        if (state.pending_stage_transition.has_value()) {
            ApplyPendingStageTransitionNow(state, graphics);
            state.scene_frame = 0;
            state.SetMode(Mode::Playing);
            return;
        }
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

void StepGameOver(
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt,
    SimulationTickMode mode
) {
    (void)mode;
    // audio
    //     .rl_audio_device
    //     .update_music_stream(&mut audio.songs[audio_asset_ids::GameOver as usize]);
    StepTransientLights(state);
    LatchPlayingInputsForTick(state);
    UpdateSpectatorTarget(state);
    UpdateControlledEnt(state);
    RefreshPlayableCharacterLamp(state);
    StepDebugLocalPlayerBots(state);
    StepPlayerSlotControls(state, graphics, audio, dt);
    state.contact.ClearEntContactDispatchesThisTick();
    state.contact.StepContactCooldowns(state.stage_frame);
    state.contact.StepInteractionCooldowns(state.stage_frame);
    state.contact.StepProjBodyImpactCooldowns(state.stage_frame);
    state.ClearWorldPrompts();
    state.ClearInteractClaims();
    state.ent_tools.Step();
    StepStageFluids(state);
    state.RebuildSid(graphics);
    SetAudioListenerWorldPos(state, GetDefaultGameplayAudioListenerWorldPos(state, graphics));
    state.stage.SyncTileShakeGrid();
    StepEnts(state, audio, graphics, dt);
    ApplyLockstepGameOverConfirm(state, graphics);
    UpdateAudioEmitters(state, audio, graphics);
    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        AttenuateEntShake(ent, kShakeAttenuationRate);
    }
    state.stage.AttenuateTileShake(kShakeAttenuationRate);
    state.particles.Step(graphics.aframe_db, dt);
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
