#include "stage_tile_triggers.hpp"

#include "state.hpp"

namespace splonks {

void RunStageTileTriggers(StageTileTriggerKind kind, const IVec2& tile_pos, State& state, Audio& audio) {
    const IVec2 resolved_tile_pos = state.stage.WrapTileCoord(tile_pos);
    if (!state.stage.IsTileCoordInside(resolved_tile_pos.x, resolved_tile_pos.y)) {
        return;
    }

    for (const StageTileTrigger& trigger : state.stage.tile_triggers) {
        if (trigger.kind != kind) {
            continue;
        }
        const IVec2 trigger_pos = state.stage.WrapTileCoord(trigger.tile_pos);
        if (!(trigger_pos == resolved_tile_pos)) {
            continue;
        }
        if (trigger.on_triggered != nullptr) {
            trigger.on_triggered(trigger, resolved_tile_pos, state, audio);
        }
    }
}

void RunStageTileDestroyedTriggers(const IVec2& tile_pos, State& state, Audio& audio) {
    RunStageTileTriggers(StageTileTriggerKind::Destroyed, tile_pos, state, audio);
}

} // namespace splonks
