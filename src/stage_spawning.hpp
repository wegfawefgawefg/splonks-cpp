#pragma once

#include "entity.hpp"
#include "player_id.hpp"
#include "state.hpp"

#include <optional>
#include <vector>

namespace splonks {

struct PlayerStageCarryover {
    PlayerId player_id = kInvalidPlayerId;
    std::optional<Entity> player;
    std::optional<Entity> held_item;
    std::optional<Entity> back_item;
    std::optional<EntityToolState> player_tools;
};

struct StageCarryover {
    std::vector<PlayerStageCarryover> players;
};

void InitCommonStageState(State& state);

StageCarryover CaptureStageCarryover(const State& state);
void RestoreStageCarryover(State& state, const StageCarryover& carryover);
void PlacePlayerAtPosition(State& state, const Vec2& pos);
void SnapAttachedItemsToPlayer(State& state);

void SpawnPlayer(State& state, const Vec2& pos);
std::optional<VID> SpawnPlayerForPlayerId(State& state, PlayerId player_id, const Vec2& pos);

std::optional<VID> SpawnStageEntityAtTopLeft(State& state, EntityType type_, const Vec2& pos);
std::optional<VID> SpawnStageEntityAtCenter(State& state, EntityType type_, const Vec2& center);
void SpawnAuthoredStageEntities(State& state);

} // namespace splonks
