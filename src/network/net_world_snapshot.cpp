#include "network/net_world_snapshot.hpp"

#include "entity.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_message.hpp"
#include "network/net_gameplay_replication.hpp"
#include "network/net_replication_payloads.hpp"
#include "network/net_session.hpp"
#include "state.hpp"
#include "tile_archetype.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace splonks::network {

namespace {

std::uint16_t AnimationFrameU16(const Entity& entity) {
    return static_cast<std::uint16_t>(std::min<std::size_t>(
        entity.frame_data_animator.current_frame,
        std::numeric_limits<std::uint16_t>::max()
    ));
}

void EnqueueTileSnapshot(State& state) {
    Stage& stage = state.stage;
    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const IVec2 tile_pos = IVec2::New(x, y);
            ReplicateTileChanged(
                state,
                GameplayTileChanged{
                    .tile_pos = tile_pos,
                    .tile = stage.GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y)),
                    .rotation = stage.GetTileRotation(
                        static_cast<unsigned int>(x),
                        static_cast<unsigned int>(y)
                    ),
                    .layer = GameplayTileLayer::Foreground,
                }
            );
            ReplicateTileChanged(
                state,
                GameplayTileChanged{
                    .tile_pos = tile_pos,
                    .tile = stage.GetBackwallTile(
                        static_cast<unsigned int>(x),
                        static_cast<unsigned int>(y)
                    ),
                    .rotation = kTileRotation0,
                    .layer = GameplayTileLayer::Backwall,
                }
            );
        }
    }
}

void EnqueueFluidSnapshot(State& state) {
    Stage& stage = state.stage;
    if (stage.tiles.empty()) {
        return;
    }
    stage.SyncTileInstanceMetadataGrid();

    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t row = static_cast<std::size_t>(y);
            const std::size_t col = static_cast<std::size_t>(x);
            Tile tile = stage.fluid_tiles[row][col];
            float amount = std::clamp(stage.fluid_amount[row][col], 0.0F, 1.0F);
            Vec2 velocity = stage.fluid_velocity[row][col];
            Vec2 temp_gravity = stage.fluid_temp_gravity[row][col];
            Vec2 gravity = stage.fluid_gravity[row][col];
            float gravity_strength = std::max(0.0F, stage.fluid_gravity_strength[row][col]);
            if (tile == Tile::Air || !GetTileArchetype(tile).simulated_fluid || amount <= 0.0001F) {
                tile = Tile::Air;
                amount = 0.0F;
                velocity = Vec2::New(0.0F, 0.0F);
                temp_gravity = Vec2::New(0.0F, 0.0F);
            }
            if (tile == Tile::Air &&
                amount == 0.0F &&
                velocity.x == 0.0F &&
                velocity.y == 0.0F &&
                gravity.x == 0.0F &&
                gravity.y == 0.0F &&
                temp_gravity.x == 0.0F &&
                temp_gravity.y == 0.0F &&
                gravity_strength == 0.0F) {
                continue;
            }

            NetMessage message;
            message.header = state.net_session.MakeLocalTransientMessageHeader(state.frame);
            message.type = NetMessageType::FluidCellPatched;
            message.payload = FluidCellPatchedMessage{
                .tile_pos = IVec2::New(x, y),
                .tile = tile,
                .amount = amount,
                .velocity = velocity,
                .gravity = gravity,
                .temp_gravity = temp_gravity,
                .gravity_strength = gravity_strength,
            };
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void EnqueueStageLightSnapshot(State& state) {
    for (const StageLight& light : state.stage.lights) {
        ReplicateStageLightAdded(
            state,
            GameplayStageLightAdded{
                .light_vid = light.vid,
                .tile_pos = light.tile_pos,
                .radius = light.radius,
            }
        );
    }
}

void EnqueueSpawnSnapshotForEntity(State& state, const Entity& entity) {
    if (!entity.active || entity.type_ == EntityType::None) {
        return;
    }

    std::optional<VID> held_by_vid = entity.held_by_vid;
    ReplicateEntitySpawned(
        state,
        GameplayEntitySpawned{
            .entity_vid = entity.vid,
            .held_by_vid = held_by_vid,
            .entity_type = entity.type_,
            .pos = entity.pos,
            .vel = entity.vel,
            .acc = entity.acc,
            .size = entity.size,
            .counter_a = entity.counter_a,
            .counter_b = entity.counter_b,
            .light_strength = entity.light_strength,
            .light_color = entity.light_color,
            .light_radius = entity.light_radius,
            .movement_flags = entity.movement_flags,
            .use_pressed = entity.use_state.pressed,
            .animate = static_cast<std::uint8_t>(entity.frame_data_animator.animate ? 1 : 0),
            .animation_loop = static_cast<std::uint8_t>(entity.frame_data_animator.loop ? 1 : 0),
            .animation_finished =
                static_cast<std::uint8_t>(entity.frame_data_animator.finished ? 1 : 0),
            .animation_id = entity.frame_data_animator.animation_id,
            .animation_frame = AnimationFrameU16(entity),
            .animation_time = entity.frame_data_animator.current_time,
            .animation_speed = entity.frame_data_animator.speed,
        }
    );
}

void EnqueueEntitySnapshot(State& state) {
    RegisterStageEntityLinks(state);

    std::vector<VID> active_entities;
    active_entities.reserve(state.entity_manager.entities.size());
    std::vector<VID> inactive_linked_entities;
    inactive_linked_entities.reserve(state.entity_manager.entities.size());
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active) {
            active_entities.push_back(entity.vid);
        } else if (state.net_session.FindNetEntityId(entity.vid).has_value()) {
            inactive_linked_entities.push_back(entity.vid);
        }
    }

    for (VID entity_vid : active_entities) {
        const Entity* const entity = state.entity_manager.GetEntity(entity_vid);
        if (entity != nullptr) {
            EnqueueSpawnSnapshotForEntity(state, *entity);
        }
    }
    for (VID entity_vid : inactive_linked_entities) {
        ReplicateEntityDeactivated(
            state,
            GameplayEntityDeactivated{
                .entity_vid = entity_vid,
            }
        );
    }
}

} // namespace

void EnqueueWorldSnapshotMessages(State& state) {
    if (state.net_session.role != NetRole::Coordinator) {
        return;
    }

    EnqueueTileSnapshot(state);
    EnqueueFluidSnapshot(state);
    EnqueueStageLightSnapshot(state);
    EnqueueEntitySnapshot(state);
}

} // namespace splonks::network
