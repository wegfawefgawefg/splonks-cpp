#include "network/net_lobby_internal.hpp"

#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "quest_stage_loader.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

std::uint32_t MakeHostStageSeed(const State& state) {
    const std::uint32_t frame_component = state.frame == 0 ? 1U : state.frame;
    return frame_component ^ 0x51A7E5D3U;
}

bool StageCanBeNetworkSynced(const State& state) {
    return !state.stage.quest_id.empty() && !state.stage.quest_stage_id.empty();
}


} // namespace

bool EnsureHostSyncedStage(State& state, std::string* status_out) {
    if (!StageCanBeNetworkSynced(state)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: current stage is not a quest stage.";
        }
        return false;
    }

    const std::string quest_id = state.stage.quest_id;
    const std::string quest_stage_id = state.stage.quest_stage_id;
    const std::uint32_t seed = state.stage.generation_seed.value_or(MakeHostStageSeed(state));
    if (!state.stage.generation_seed.has_value()) {
        if (!LoadQuestStage(state, quest_id, quest_stage_id, false, seed)) {
            if (status_out != nullptr) {
                *status_out = "Host failed: could not reload current quest stage with sync seed.";
            }
            return false;
        }
    }

    state.net_session.quest_id = quest_id;
    state.net_session.quest_stage_id = quest_stage_id;
    state.net_session.stage_seed = seed;
    return true;
}

bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No synced network stage is active.";
        }
        return false;
    }
    if (state.net_session.quest_id.empty() || state.net_session.quest_stage_id.empty()) {
        if (status_out != nullptr) {
            *status_out = "No synced quest stage metadata is available.";
        }
        return false;
    }
    struct SavedPlayerSlot {
        PlayerId player_id = kInvalidPlayerId;
        bool local = false;
        bool primary = false;
    };
    std::vector<SavedPlayerSlot> saved_slots;
    saved_slots.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        saved_slots.push_back(SavedPlayerSlot{
            .player_id = slot.player_id,
            .local = slot.connection_kind == PlayerConnectionKind::Local,
            .primary = slot.primary_local,
        });
    }

    state.players = PlayerRegistry::New();
    if (state.net_transport) {
        state.net_transport->remote_player_targets.clear();
        state.net_transport->replicated_entity_state_cache.clear();
        state.net_transport->replicated_fluid_cell_cache.clear();
    }
    const bool loaded = LoadQuestStage(
        state,
        state.net_session.quest_id,
        state.net_session.quest_stage_id,
        false,
        state.net_session.stage_seed
    );
    if (!loaded) {
        if (status_out != nullptr) {
            *status_out = "Synced stage reload failed.";
        }
        return false;
    }
    RegisterStageEntityLinks(state);
    const Vec2 spawn_base = GetPrimaryPlayerSpawnPos(state);
    state.players.slots.clear();
    state.controlled_entity_vid.reset();
    for (std::size_t i = 0; i < saved_slots.size(); ++i) {
        const SavedPlayerSlot& slot = saved_slots[i];
        EnsureSpawnedPlayer(
            state,
            slot.player_id,
            slot.local,
            slot.primary,
            spawn_base + Vec2::New(static_cast<float>(i) * 8.0F, 0.0F),
            graphics
        );
    }

    if (status_out != nullptr) {
        *status_out = "Reloaded synced stage " + state.net_session.quest_stage_id +
                      " seed " + std::to_string(state.net_session.stage_seed) + ".";
    }
    return true;
}

} // namespace splonks::network
