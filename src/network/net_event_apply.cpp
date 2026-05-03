#include "network/net_event_apply.hpp"

#include "audio_emitters.hpp"
#include "network/net_event.hpp"
#include "network/net_session.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "on_damage_effects.hpp"
#include "stage_break.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace splonks::network {

namespace {

std::uint32_t AddSignedClamped(std::uint32_t value, std::int32_t delta) {
    const std::int64_t next = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(delta);
    const std::int64_t clamped =
        std::clamp<std::int64_t>(next, 0, std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(clamped);
}

void SetTileIfChanged(State& state, const IVec2& tile_pos, Tile tile) {
    const IVec2 wrapped_pos = state.stage.WrapTileCoord(tile_pos);
    if (!state.stage.IsTileCoordInside(wrapped_pos.x, wrapped_pos.y)) {
        return;
    }
    if (state.stage.GetTile(
            static_cast<unsigned int>(wrapped_pos.x),
            static_cast<unsigned int>(wrapped_pos.y)
        ) == tile) {
        return;
    }
    state.stage.SetTile(wrapped_pos, tile);
    const std::vector<IVec2> changed_tiles{wrapped_pos};
    UpdateStageLightingForTileChanges(state, changed_tiles);
    UpdateStageAcousticsForTileChanges(state, changed_tiles);
}

std::optional<VID> FindEntityVidForEvent(
    NetSessionState& session,
    State& state,
    NetEntityId entity_id
);

bool IsAttachmentDriven(const Entity& entity) {
    return entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None;
}

void ApplyEntitySpawnedEvent(
    NetSessionState& session,
    State& state,
    const EntitySpawnedEvent& payload,
    Graphics* graphics
) {
    if (payload.entity_id == kInvalidNetEntityId || payload.entity_type == EntityType::None) {
        return;
    }

    if (session.FindLocalVid(payload.entity_id).has_value()) {
        return;
    }

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, payload.entity_type);
    entity->pos = payload.pos;
    entity->vel = payload.vel;
    entity->acc = Vec2::New(0.0F, 0.0F);
    entity->counter_a = payload.counter_a;
    entity->counter_b = payload.counter_b;
    if (payload.held_by_id != kInvalidNetEntityId) {
        if (const std::optional<VID> holder_vid =
                FindEntityVidForEvent(session, state, payload.held_by_id)) {
            const Entity* const holder = state.entity_manager.GetEntity(*holder_vid);
            if (holder != nullptr && holder->active) {
                entity->held_by_vid = holder->vid;
                entity->attachment_mode = AttachmentMode::Held;
                entity->has_physics = false;
                entity->can_collide = false;
            }
        }
    }
    if (payload.use_pressed) {
        UseEntity(*entity, std::nullopt, AttachmentMode::None);
    }
    session.LinkEntity(payload.entity_id, entity->vid);
    session.SetEntityOwner(payload.entity_id, payload.owner.player_id);
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

std::optional<VID> FindEntityVidForEvent(
    NetSessionState& session,
    State& state,
    NetEntityId entity_id
) {
    if (const std::optional<VID> linked = session.FindLocalVid(entity_id)) {
        return linked;
    }
    if (IsPlayerNetEntityId(entity_id)) {
        const PlayerId player_id = GetPlayerIdFromNetEntityId(entity_id);
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot == nullptr || !slot->entity_vid.has_value()) {
            return std::nullopt;
        }
        if (const Entity* const entity = state.entity_manager.GetEntity(*slot->entity_vid)) {
            if (!entity->active) {
                return std::nullopt;
            }
        }
        session.LinkEntity(entity_id, *slot->entity_vid);
        return slot->entity_vid;
    }
    if (entity_id == kInvalidNetEntityId ||
        entity_id > static_cast<NetEntityId>(state.entity_manager.entities.size())) {
        return std::nullopt;
    }

    const std::size_t entity_idx = static_cast<std::size_t>(entity_id - 1U);
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active) {
        return std::nullopt;
    }

    session.LinkEntity(entity_id, entity.vid);
    return entity.vid;
}

void ApplyEntityDamagedEvent(
    NetSessionState& session,
    State& state,
    Audio* audio,
    PlayerId source_player_id,
    const EntityDamagedEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    if (!IsPlayerNetEntityId(payload.entity_id)) {
        session.SetEntityOwner(payload.entity_id, source_player_id);
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    const PlayerSlot* const target_player_slot = state.players.FindByEntityVid(entity->vid);
    const bool target_is_local_player =
        target_player_slot != nullptr &&
        target_player_slot->connection_kind == PlayerConnectionKind::Local;
    if (target_is_local_player && target_player_slot->player_id != source_player_id) {
        // Remote simulations can have stale/enemy-overlap disagreement for our
        // local player body. Local players own their own damage; remote attacks
        // should replicate through their spawned/projectile entities and be
        // resolved by this machine's simulation.
        return;
    }

    const bool attachment_driven = IsAttachmentDriven(*entity);
    if (!attachment_driven) {
        entity->pos = payload.pos;
        entity->vel = payload.vel;
        entity->acc = payload.acc;
        entity->grounded = payload.grounded != 0;
    }
    entity->stun_timer = payload.stun_timer;

    if (payload.remaining_health == 0) {
        entity->health = 0;
        if (audio != nullptr) {
            entities::common::DieIfDead(entity->vid.id, state, *audio);
        } else {
            entity->condition = EntityCondition::Dead;
        }
        return;
    }

    entity->health = payload.remaining_health;
    entity->condition = static_cast<EntityCondition>(payload.condition);
    if (entity->condition == EntityCondition::Stunned ||
        entity->condition == EntityCondition::Dead) {
        entities::common::ReleaseEntityFromHolder(*entity, state);
    }
    if (audio != nullptr && payload.amount > 0) {
        if (entity->damage_animation.has_value()) {
            SpawnDamageEffectAnimationBurst(*entity->damage_animation, entity->GetCenter(), state);
        }
        if (entity->damage_sound.has_value()) {
            (void)PlayEntityCenterSoundEmitter(state, *entity, *entity->damage_sound);
        }
    }
}

void ApplyEntityStatePatchedEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    PlayerId source_player_id,
    const EntityStatePatchedEvent& payload
) {
    const std::optional<PlayerId> owner_player_id = session.FindEntityOwner(payload.entity_id);
    if (owner_player_id.has_value()) {
        if (*owner_player_id == session.local_player_id ||
            *owner_player_id != source_player_id) {
            return;
        }
    } else if (source_player_id != session.coordinator_player_id) {
        return;
    }

    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }
    if (payload.active == 0) {
        state.entity_manager.SetInactive(entity->vid.id);
        return;
    }
    const bool attachment_driven = IsAttachmentDriven(*entity);
    if (!attachment_driven) {
        entity->pos = payload.pos;
        entity->vel = payload.vel;
        entity->acc = payload.acc;
        entity->grounded = payload.grounded != 0;
    }
    entity->health = payload.health;
    entity->stun_timer = payload.stun_timer;
    entity->condition = static_cast<EntityCondition>(payload.condition);
    state.stage.NormalizeEntityPositionForWrap(*entity);
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

bool IsVidInHolderChain(VID needle, const Entity& entity, const State& state) {
    std::optional<VID> holder_vid = entity.held_by_vid;
    constexpr int kMaxCarryChainDepth = 16;
    for (int depth = 0; depth < kMaxCarryChainDepth && holder_vid.has_value(); ++depth) {
        if (*holder_vid == needle) {
            return true;
        }
        const Entity* const holder = state.entity_manager.GetEntity(*holder_vid);
        if (holder == nullptr || !holder->active) {
            return false;
        }
        holder_vid = holder->held_by_vid;
    }
    return false;
}

bool IsImmediateLocalGameplayEvent(NetEventType type) {
    switch (type) {
    case NetEventType::EntitySpawned:
    case NetEventType::EntityHeld:
    case NetEventType::EntityDropped:
    case NetEventType::EntityThrown:
    case NetEventType::EntityDamaged:
    case NetEventType::TileBroken:
    case NetEventType::RopeTilePlaced:
    case NetEventType::TileChanged:
        return true;
    default:
        return false;
    }
}

void ApplyEntityHeldEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityHeldEvent& payload
) {
    const std::optional<VID> holder_vid = FindEntityVidForEvent(session, state, payload.holder_id);
    const std::optional<VID> held_vid = FindEntityVidForEvent(session, state, payload.held_id);
    if (!holder_vid.has_value() || !held_vid.has_value() || *holder_vid == *held_vid) {
        return;
    }
    session.SetEntityOwner(payload.held_id, session.FindEntityOwner(payload.holder_id));

    Entity* const holder = state.entity_manager.GetEntityMut(*holder_vid);
    Entity* const held = state.entity_manager.GetEntityMut(*held_vid);
    if (holder == nullptr || held == nullptr || !holder->active || !held->active) {
        return;
    }
    if (held->held_by_vid.has_value() && *held->held_by_vid != holder->vid) {
        return;
    }
    if (held->attachment_mode != AttachmentMode::None &&
        (!held->held_by_vid.has_value() || *held->held_by_vid != holder->vid)) {
        return;
    }
    if (holder->holding_vid.has_value() && *holder->holding_vid != held->vid) {
        return;
    }
    if (IsVidInHolderChain(held->vid, *holder, state)) {
        return;
    }

    entities::common::AttachEntityAsHeld(*holder, *held);
    if (graphics != nullptr) {
        entities::common::SyncEntityAttachments(holder->vid.id, state, *graphics);
        state.UpdateSidForEntity(holder->vid.id, *graphics);
        state.UpdateSidForEntity(held->vid.id, *graphics);
    }
}

void ApplyEntityDroppedEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityDroppedEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);
    if (IsPlayerNetEntityId(payload.entity_id)) {
        const PlayerId player_id = GetPlayerIdFromNetEntityId(payload.entity_id);
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot != nullptr && slot->connection_kind == PlayerConnectionKind::Local) {
            if (graphics != nullptr) {
                state.UpdateSidForEntity(entity->vid.id, *graphics);
            }
            return;
        }
    }
    entity->pos = payload.pos;
    entity->vel = payload.vel;
    entity->acc = Vec2::New(0.0F, 0.0F);
    entity->grounded = false;
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

void ApplyEntityThrownEvent(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const EntityThrownEvent& payload
) {
    const std::optional<VID> vid = FindEntityVidForEvent(session, state, payload.entity_id);
    if (!vid.has_value()) {
        return;
    }
    session.SetEntityOwner(payload.entity_id, session.FindEntityOwner(payload.thrower_id));
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr || !entity->active) {
        return;
    }

    std::optional<VID> thrower_vid;
    if (payload.thrower_id != kInvalidNetEntityId) {
        thrower_vid = FindEntityVidForEvent(session, state, payload.thrower_id);
    }

    entities::common::ReleaseEntityFromHolder(*entity, state);
    entity->pos = payload.pos;
    entity->vel = payload.vel;
    entity->acc = Vec2::New(0.0F, 0.0F);
    entity->thrown_by = thrower_vid;
    entity->thrown_immunity_timer = entities::common::kThrownByImmunityDuration;
    const EntityArchetype& archetype = GetEntityArchetype(entity->type_);
    entity->can_apply_projectile_contact = archetype.can_apply_projectile_contact;
    entity->projectile_contact_damage_type = archetype.projectile_contact_damage_type;
    entity->projectile_contact_damage_amount = archetype.projectile_contact_damage_amount;
    entity->projectile_contact_timer = entities::common::kProjectileContactDuration;
    entity->grounded = false;
    if (graphics != nullptr) {
        state.UpdateSidForEntity(entity->vid.id, *graphics);
    }
}

} // namespace

std::size_t ApplyOrderedEvents(
    NetSessionState& session,
    State& state,
    Audio* audio,
    Graphics* graphics
) {
    std::size_t applied_count = 0;
    std::vector<NetEventId> transient_applied_event_ids;
    for (const NetEvent& event : session.ordered_events) {
        if (session.HasAppliedEvent(event.header.event_id)) {
            continue;
        }
        if (event.header.source_player_id == session.local_player_id &&
            IsImmediateLocalGameplayEvent(event.type)) {
            if (session.MarkEventApplied(event.header.event_id)) {
                session.AddEventLog(NetEventLogPhase::SkippedLocal, event);
                ++applied_count;
            }
            continue;
        }

        switch (event.type) {
        case NetEventType::EntitySpawned:
            if (const auto* payload = std::get_if<EntitySpawnedEvent>(&event.payload)) {
                ApplyEntitySpawnedEvent(session, state, *payload, graphics);
            }
            break;
        case NetEventType::EntityDamaged:
            if (const auto* payload = std::get_if<EntityDamagedEvent>(&event.payload)) {
                ApplyEntityDamagedEvent(session, state, audio, event.header.source_player_id, *payload);
            }
            break;
        case NetEventType::EntityStatePatched:
            if (const auto* payload = std::get_if<EntityStatePatchedEvent>(&event.payload)) {
                ApplyEntityStatePatchedEvent(
                    session,
                    state,
                    graphics,
                    event.header.source_player_id,
                    *payload
                );
            }
            break;
        case NetEventType::EntityHeld:
            if (const auto* payload = std::get_if<EntityHeldEvent>(&event.payload)) {
                ApplyEntityHeldEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::EntityDropped:
            if (const auto* payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
                ApplyEntityDroppedEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::EntityThrown:
            if (const auto* payload = std::get_if<EntityThrownEvent>(&event.payload)) {
                ApplyEntityThrownEvent(session, state, graphics, *payload);
            }
            break;
        case NetEventType::MoneyChanged:
            if (const auto* payload = std::get_if<MoneyChangedEvent>(&event.payload)) {
                state.points = AddSignedClamped(state.points, payload->delta);
            }
            break;
        case NetEventType::FavorChanged:
            if (const auto* payload = std::get_if<FavorChangedEvent>(&event.payload)) {
                state.sac_altar_favor += payload->delta;
            }
            break;
        case NetEventType::TileBroken:
            if (const auto* payload = std::get_if<TileBrokenEvent>(&event.payload)) {
                if (audio != nullptr) {
                    BreakStageTilesAtCoords(
                        {payload->tile_pos},
                        state,
                        *audio,
                        std::nullopt,
                        false,
                        true
                    );
                } else {
                    const IVec2 tile_pos = state.stage.WrapTileCoord(payload->tile_pos);
                    if (state.stage.IsTileCoordInside(tile_pos.x, tile_pos.y)) {
                        state.stage.SetTile(tile_pos, Tile::Air);
                        const std::vector<IVec2> changed_tiles{tile_pos};
                        UpdateStageLightingForTileChanges(state, changed_tiles);
                        UpdateStageAcousticsForTileChanges(state, changed_tiles);
                    }
                }
            }
            break;
        case NetEventType::RopeTilePlaced:
            if (const auto* payload = std::get_if<RopeTilePlacedEvent>(&event.payload)) {
                SetTileIfChanged(state, payload->tile_pos, Tile::Rope);
            }
            break;
        case NetEventType::TileChanged:
            if (const auto* payload = std::get_if<TileChangedEvent>(&event.payload)) {
                SetTileIfChanged(state, payload->tile_pos, payload->tile);
            }
            break;
        default:
            break;
        }

        if (session.MarkEventApplied(event.header.event_id)) {
            session.AddEventLog(NetEventLogPhase::Applied, event);
            ++applied_count;
            if (event.type == NetEventType::EntityStatePatched) {
                transient_applied_event_ids.push_back(event.header.event_id);
            }
        }
    }
    if (!transient_applied_event_ids.empty()) {
        session.ordered_events.erase(
            std::remove_if(
                session.ordered_events.begin(),
                session.ordered_events.end(),
                [&](const NetEvent& event) {
                    return event.type == NetEventType::EntityStatePatched &&
                           std::find(
                               transient_applied_event_ids.begin(),
                               transient_applied_event_ids.end(),
                               event.header.event_id
                           ) != transient_applied_event_ids.end();
                }
            ),
            session.ordered_events.end()
        );
        session.applied_event_ids.erase(
            std::remove_if(
                session.applied_event_ids.begin(),
                session.applied_event_ids.end(),
                [&](NetEventId event_id) {
                    return std::find(
                               transient_applied_event_ids.begin(),
                               transient_applied_event_ids.end(),
                               event_id
                           ) != transient_applied_event_ids.end();
                }
            ),
            session.applied_event_ids.end()
        );
    }
    return applied_count;
}

} // namespace splonks::network
