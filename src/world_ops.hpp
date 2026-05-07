#pragma once

#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "presentation_commands.hpp"
#include "stage.hpp"
#include "vid.hpp"

#include <functional>
#include <optional>

namespace splonks {

struct Entity;
struct GameplayActionRequested;
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
void PatchEntityState(State& state, const Entity& source, const Entity& entity);
void PatchPlayerState(State& state, const Entity& player);
void PatchRunState(State& state);
void MarkEntityHeld(
    State& state,
    const Entity& holder,
    const Entity& held,
    AttachmentMode attachment_mode = AttachmentMode::Held
);
void MarkEntityDropped(
    State& state,
    const Entity& entity,
    std::optional<VID> dropped_by_vid = std::nullopt
);
void MarkEntityThrown(State& state, const Entity& thrower, const Entity& thrown, Vec2 throw_velocity);
void CommitEntityDamaged(
    State& state,
    const Entity& entity,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid = std::nullopt
);
void QueuePresentationCommand(State& state, const PresentationCommand& command);
void RequestGameplayAction(State& state, const GameplayActionRequested& action);
void QueuePendingGameplayAction(State& state, const GameplayActionRequested& action);
bool TryRequestOrApplyInteractEntity(
    VID source_vid,
    VID target_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void ProcessPendingGameplayActions(State& state, Graphics& graphics, Audio& audio);

bool SetForegroundTile(
    State& state,
    const IVec2& tile_pos,
    Tile tile,
    TileRotation rotation = kTileRotation0,
    bool allow_peer_canonical_apply = false
);

bool PlaceRopeTile(
    State& state,
    const Entity& source_entity,
    const IVec2& tile_pos,
    bool allow_peer_canonical_apply = false
);
void CommitTileBroken(State& state, const IVec2& tile_pos);

} // namespace world_ops

} // namespace splonks
