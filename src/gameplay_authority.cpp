#include "gameplay_authority.hpp"

#include "entity.hpp"
#include "state.hpp"

#include <optional>

namespace splonks {

bool HasLocalGameplayAuthorityForEntity(const State& state, VID entity_vid) {
    return state.net_session.HasLocalAuthorityForEntity(entity_vid);
}

bool HasLocalGameplayAuthorityForInteractionSource(const State& state, VID entity_vid) {
    if (state.net_session.role == network::NetRole::Offline) {
        return true;
    }
    if (state.net_session.HasLocalAuthorityForEntity(entity_vid)) {
        return true;
    }

    constexpr int kMaxHolderChainDepth = 16;
    std::optional<VID> cursor = entity_vid;
    for (int depth = 0; depth < kMaxHolderChainDepth && cursor.has_value(); ++depth) {
        if (const PlayerSlot* const player_slot = state.players.FindByEntityVid(*cursor)) {
            return player_slot->connection_kind == PlayerConnectionKind::Local;
        }

        const Entity* const entity = state.entity_manager.GetEntity(*cursor);
        if (entity == nullptr || !entity->active) {
            return false;
        }
        if (!entity->held_by_vid.has_value()) {
            return false;
        }
        cursor = entity->held_by_vid;
    }

    return state.net_session.HasLocalAuthorityForEntity(entity_vid);
}

} // namespace splonks
