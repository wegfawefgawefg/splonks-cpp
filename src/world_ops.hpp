#pragma once

#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "stage.hpp"
#include "vid.hpp"

#include <functional>
#include <optional>

namespace splonks {

struct Entity;
struct Audio;
struct Graphics;
struct State;

namespace world_ops {

using EntitySpawnSetup = std::function<void(Entity&)>;

Entity* SpawnEntity(
    State& state,
    EntityType type_,
    const EntitySpawnSetup& setup = {},
    std::optional<VID> held_by_vid = std::nullopt
);
Entity* SpawnConfiguredEntity(
    State& state,
    const EntitySpawnSetup& setup,
    std::optional<VID> held_by_vid = std::nullopt
);
bool DeactivateEntity(State& state, VID entity_vid);
bool TryApplyInteractEntity(
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
    const Entity& source_entity,
    const IVec2& tile_pos
);

} // namespace world_ops

} // namespace splonks
