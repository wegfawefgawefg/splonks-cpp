#pragma once

#include "ent.hpp"
#include "player_id.hpp"
#include "state.hpp"

#include <optional>
#include <vector>

namespace splonks {

struct PlayerStageCarryover {
    PlayerId player_id = kInvalidPlayerId;
    std::optional<Ent> player;
    std::optional<Ent> held_item;
    std::optional<Ent> back_item;
    std::optional<EntToolState> player_tools;
};

struct StageCarryover {
    std::vector<PlayerStageCarryover> players;
};

void InitCommonStageState(State& state);

StageCarryover CaptureStageCarryover(const State& state);
void RestoreStageCarryover(State& state, const StageCarryover& carryover);
void PlacePlayerAtPosition(State& state, FxVec2 pos);
void PlacePlayerAtAuthoredPosition(State& state, const FVec2& pos);
void SnapAttachedItemsToPlayer(State& state);

void SpawnPlayer(State& state, FxVec2 pos);
void SpawnPlayerAtAuthoredPosition(State& state, const FVec2& pos);
std::optional<VID> SpawnPlayerForPlayerId(State& state, PlayerId player_id, FxVec2 pos);
std::optional<VID> SpawnPlayerForPlayerIdAtAuthoredPosition(
    State& state,
    PlayerId player_id,
    const FVec2& pos
);

std::optional<VID> SpawnStageEntAtTopLeft(State& state, EntType type_, FxVec2 pos);
std::optional<VID> SpawnStageEntAtAuthoredTopLeft(State& state, EntType type_, const FVec2& pos);
std::optional<VID> SpawnStageEntAtCenter(State& state, EntType type_, FxVec2 center);
std::optional<VID> SpawnStageEntAtAuthoredCenter(State& state, EntType type_, const FVec2& center);
void SpawnAuthoredStageEnts(State& state);

} // namespace splonks
