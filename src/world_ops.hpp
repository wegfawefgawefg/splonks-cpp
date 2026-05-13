#pragma once

#include "ent/core_types.hpp"
#include "math_types.hpp"
#include "stage.hpp"
#include "vid.hpp"

#include <functional>
#include <optional>

namespace splonks {

struct Ent;
struct Audio;
struct Graphics;
struct State;

namespace world_ops {

using EntSpawnSetup = std::function<void(Ent&)>;

Ent* SpawnEnt(
    State& state,
    EntType type_,
    const EntSpawnSetup& setup = {},
    std::optional<VID> held_by_vid = std::nullopt
);
Ent* SpawnConfiguredEnt(
    State& state,
    const EntSpawnSetup& setup,
    std::optional<VID> held_by_vid = std::nullopt
);
bool DeactivateEnt(State& state, VID ent_vid);
bool TryApplyInteractEnt(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
);

bool SetForegroundTile(
    State& state,
    const IVec2& tile_pos,
    Tile tile,
    TileRotation rotation = kTileRotation0
);

bool PlaceRopeTile(
    State& state,
    const Ent& source_ent,
    const IVec2& tile_pos
);

} // namespace world_ops

} // namespace splonks
