#include "entities/common/common.hpp"

#include "entity/archetype.hpp"
#include "entities/player.hpp"
#include "tile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace splonks::entities::common {

namespace {

AABB GetAabbAtPosition(const Entity& entity, const Vec2& pos) {
    return AABB::New(pos, pos + entity.size - Vec2::New(1.0F, 1.0F));
}

void StoreDistanceTraveled(std::size_t entity_idx, State& state, const Vec2& start_pos) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    float dist_traveled = Length(entity.pos - start_pos);
    if (dist_traveled < 1.0F) {
        dist_traveled = 0.0F;
    }
    entity.dist_traveled_this_frame = dist_traveled;
}

void ResolveBlockingOverlap(
    std::size_t entity_idx,
    State& state,
    bool check_tiles,
    bool check_entities
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const AABB current_aabb = entity.GetAABB();
    const BlockingContactSet current_contacts =
        GatherBlockingContactsForAabb(entity_idx, current_aabb, state, check_tiles, check_entities);
    if (!ResolveBlockingContactSet(entity_idx, current_contacts, state).blocks_movement) {
        return;
    }

    const int max_push = static_cast<int>(kTileSize) * 2;
    const std::array<IVec2, 4> candidates = {
        IVec2::New(0, -1),
        IVec2::New(-1, 0),
        IVec2::New(1, 0),
        IVec2::New(0, 1),
    };

    for (int distance = 1; distance <= max_push; ++distance) {
        for (const IVec2& direction : candidates) {
            const Vec2 candidate_pos = entity.pos + ToVec2(direction * distance);
            const AABB candidate_aabb = GetAabbAtPosition(entity, candidate_pos);
            const BlockingContactSet candidate_contacts = GatherBlockingContactsForAabb(
                entity_idx, candidate_aabb, state, check_tiles, check_entities);
            if (!ResolveBlockingContactSet(entity_idx, candidate_contacts, state).blocks_movement) {
                entity.pos = candidate_pos;
                return;
            }
        }
    }
}

bool HasBlockingTileContact(const BlockingContactSet& contacts) {
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.tile != nullptr && IsTileCollidable(*tile_contact.tile)) {
            return true;
        }
    }
    return false;
}

BlockingImpactSurface GetImpactSurfaceForBlockedContacts(const BlockingContactSet& contacts) {
    if (HasBlockingTileContact(contacts)) {
        return BlockingImpactSurface::Tiles;
    }
    if (contacts.touches_stage_bounds) {
        return BlockingImpactSurface::StageBounds;
    }
    return BlockingImpactSurface::ImpassableEntity;
}

int GetIntegerStepDistance(float distance, unsigned int time) {
    const float abs_distance = std::abs(distance);
    int integer_distance = static_cast<int>(std::floor(abs_distance));
    const float fractional_distance = abs_distance - static_cast<float>(integer_distance);
    if (fractional_distance != 0.0F) {
        const int fractional_period = static_cast<int>(std::round(1.0F / fractional_distance));
        if (fractional_period != 0 && (time % static_cast<unsigned int>(fractional_period)) == 0U) {
            integer_distance += 1;
        }
    }
    if (distance < 0.0F) {
        integer_distance *= -1;
    }
    return integer_distance;
}

bool DispatchPostSweepEntityOverlapContacts(
    std::size_t entity_idx,
    State& state,
    Graphics* graphics,
    Audio* audio,
    BlockingImpactAxis impact_axis,
    int direction
) {
    if (graphics == nullptr || audio == nullptr) {
        return false;
    }
    return TryDispatchEntityEntityOverlapContacts(
        entity_idx,
        state,
        *graphics,
        *audio,
        ContactContext{
            .phase = ContactPhase::SweptEntered,
            .has_impact = false,
            .impact_axis = impact_axis,
            .direction = direction,
            .mover_vid = state.entity_manager.entities[entity_idx].vid,
        }
    );
}

void MoveEntityPixelStep(
    std::size_t entity_idx,
    State& state,
    bool check_tiles,
    bool check_entities,
    Graphics* graphics,
    Audio* audio
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const Vec2 start_pos = entity.pos;

    ResolveBlockingOverlap(entity_idx, state, check_tiles, check_entities);

    const int move_x = GetIntegerStepDistance(entity.vel.x, state.stage_frame);
    const int move_y = GetIntegerStepDistance(entity.vel.y, state.stage_frame);

    if (move_x > 0) {
        for (int i = 0; i < move_x; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(1.0F, 0.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            const BlockingContactSet contacts =
                GatherBlockingContactsForAabb(entity_idx, next_aabb, state, check_tiles, check_entities);
            const ContactResolution contact_resolution =
                ResolveBlockingContactSet(entity_idx, contacts, state);
            if (contact_resolution.stop_sweep) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
            if (contact_resolution.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Horizontal,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = entity.vel.x,
                    .direction = 1,
                    .mover_vid = entity.vid,
                };
                const ContactResolution tile_resolution =
                    TryDispatchEntityTileContacts(entity_idx, contacts, blocked_context, state, audio);
                const ContactResolution entity_resolution = TryDispatchEntityEntityContacts(
                    entity_idx,
                    contacts.entity_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (tile_resolution.stop_sweep || entity_resolution.stop_sweep) {
                    entity.collided = true;
                    entity.vel.x = 0.0F;
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (DispatchPostSweepEntityOverlapContacts(
                    entity_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Horizontal,
                    1)) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
        }
    } else if (move_x < 0) {
        for (int i = 0; i < -move_x; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(-1.0F, 0.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            const BlockingContactSet contacts =
                GatherBlockingContactsForAabb(entity_idx, next_aabb, state, check_tiles, check_entities);
            const ContactResolution contact_resolution =
                ResolveBlockingContactSet(entity_idx, contacts, state);
            if (contact_resolution.stop_sweep) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
            if (contact_resolution.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Horizontal,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = entity.vel.x,
                    .direction = -1,
                    .mover_vid = entity.vid,
                };
                const ContactResolution tile_resolution =
                    TryDispatchEntityTileContacts(entity_idx, contacts, blocked_context, state, audio);
                const ContactResolution entity_resolution = TryDispatchEntityEntityContacts(
                    entity_idx,
                    contacts.entity_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (tile_resolution.stop_sweep || entity_resolution.stop_sweep) {
                    entity.collided = true;
                    entity.vel.x = 0.0F;
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (DispatchPostSweepEntityOverlapContacts(
                    entity_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Horizontal,
                    -1)) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
        }
    }

    if (move_y > 0) {
        for (int i = 0; i < move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, 1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            const BlockingContactSet contacts =
                GatherBlockingContactsForAabb(entity_idx, next_aabb, state, check_tiles, check_entities);
            const ContactResolution contact_resolution =
                ResolveBlockingContactSet(entity_idx, contacts, state);
            if (contact_resolution.stop_sweep) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
            if (contact_resolution.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Vertical,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = entity.vel.y,
                    .direction = 1,
                    .mover_vid = entity.vid,
                };
                const ContactResolution tile_resolution =
                    TryDispatchEntityTileContacts(entity_idx, contacts, blocked_context, state, audio);
                const ContactResolution entity_resolution = TryDispatchEntityEntityContacts(
                    entity_idx,
                    contacts.entity_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (tile_resolution.stop_sweep || entity_resolution.stop_sweep) {
                    entity.collided = true;
                    entity.vel.y = 0.0F;
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (DispatchPostSweepEntityOverlapContacts(
                    entity_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Vertical,
                    1)) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
        }
    } else if (move_y < 0) {
        for (int i = 0; i < -move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, -1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            const BlockingContactSet contacts =
                GatherBlockingContactsForAabb(entity_idx, next_aabb, state, check_tiles, check_entities);
            const ContactResolution contact_resolution =
                ResolveBlockingContactSet(entity_idx, contacts, state);
            if (contact_resolution.stop_sweep) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
            if (contact_resolution.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Vertical,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = entity.vel.y,
                    .direction = -1,
                    .mover_vid = entity.vid,
                };
                const ContactResolution tile_resolution =
                    TryDispatchEntityTileContacts(entity_idx, contacts, blocked_context, state, audio);
                const ContactResolution entity_resolution = TryDispatchEntityEntityContacts(
                    entity_idx,
                    contacts.entity_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (tile_resolution.stop_sweep || entity_resolution.stop_sweep) {
                    entity.collided = true;
                    entity.vel.y = 0.0F;
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (DispatchPostSweepEntityOverlapContacts(
                    entity_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Vertical,
                    -1)) {
                StoreDistanceTraveled(entity_idx, state, start_pos);
                return;
            }
        }
    }

    if (DispatchPostSweepEntityOverlapContacts(
            entity_idx,
            state,
            graphics,
            audio,
            BlockingImpactAxis::Horizontal,
            0)) {
        StoreDistanceTraveled(entity_idx, state, start_pos);
        return;
    }

    StoreDistanceTraveled(entity_idx, state, start_pos);
}

bool IsGroundedOnEntities(std::size_t entity_idx, State& state) {
    const auto [entity_tl, entity_br] = state.entity_manager.entities[entity_idx].GetBounds();
    const VID vid = state.entity_manager.entities[entity_idx].vid;
    const Vec2 feet_tl = Vec2::New(entity_tl.x, entity_br.y);
    const Vec2 feet_br = entity_br + Vec2::New(0.0F, 1.0F);
    const AABB feet_aabb = {
        .tl = feet_tl,
        .br = feet_br,
    };
    const std::vector<VID> entities_at_feet =
        state.sid.QueryExclude(feet_aabb.tl, feet_aabb.br, vid);

    const bool impassable_entities = std::any_of(
        entities_at_feet.begin(),
        entities_at_feet.end(),
        [&](const VID& test_vid) { return state.entity_manager.entities[test_vid.id].impassable; });

    return impassable_entities;
}

} // namespace

void EulerStep(std::size_t entity_idx, State& state, float dt) {
    PrePartialEulerStep(entity_idx, state, dt);
    MoveEntityPixelStep(entity_idx, state, false, false, nullptr, nullptr);
    PostPartialEulerStep(entity_idx, state, dt);
}

void PrePartialEulerStep(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel += entity.acc;
}

void ApplyGravity(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.grounded) {
        if (entity.vel.y > 0.0F) {
            entity.vel.y = 0.0F;
        }
        return;
    }
    entity.acc.y += state.stage.gravity;
}

void PostPartialEulerStep(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel.x = std::clamp(entity.vel.x, -kMaxSpeed, kMaxSpeed);
    entity.vel.y = std::clamp(entity.vel.y, -kMaxSpeed, kMaxSpeed);
    entity.acc = Vec2::New(0.0F, 0.0F);
}

void ApplyGroundFriction(std::size_t entity_idx, State& state) {
    if (state.stage.stage_type == StageType::Ice1 || state.stage.stage_type == StageType::Ice2 ||
        state.stage.stage_type == StageType::Ice3) {
        return;
    }
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        entity.grounded = false;
    }

    if (IsGroundedOnEntities(entity_idx, state)) {
        state.entity_manager.entities[entity_idx].grounded |= true;
    }
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.SetGrounded(state.stage);
    if (entity.grounded) {
        entity.vel.x *= 0.85F;
    }
}

void ApplyArchetypeGroundFriction(std::size_t entity_idx, State& state) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!GetEntityArchetype(entity.type_).has_ground_friction) {
        return;
    }
    ApplyGroundFriction(entity_idx, state);
}

void StepStandardPhysics(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    ApplyGravity(entity_idx, state, dt);
    PrePartialEulerStep(entity_idx, state, dt);
    DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    ApplyArchetypeGroundFriction(entity_idx, state);
    PostPartialEulerStep(entity_idx, state, dt);
}

void GroundedCheck(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    bool check_tiles,
    bool check_entities
) {
    (void)audio;
    bool grounded = false;
    if (check_tiles) {
        grounded |= IsGroundedOnTiles(entity_idx, state);
    }
    if (check_entities) {
        grounded |= IsGroundedOnEntities(entity_idx, state);
    }

    Entity& entity = state.entity_manager.entities[entity_idx];

    entity.grounded = grounded;
    if (entity.grounded) {
        if (entity.vel.y > 0.0F) {
            entity.vel.y = 0.0F;
        }
        entity.coyote_time = player::kCoyoteTimeFrames;
    } else if (entity.coyote_time > 0) {
        entity.coyote_time -= 1;
    }
}

bool IsGroundedOnTiles(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];

    const auto [entity_tl, entity_br] = entity.GetBounds();
    const Vec2 feet_tl = Vec2::New(entity_tl.x, entity_br.y);
    const Vec2 feet_br = entity_br + Vec2::New(0.0F, 1.0F);
    if (feet_br.y >= static_cast<float>(state.stage.GetHeight()) &&
        state.stage.IsBorderSideBlocking(StageBorderSideKind::Bottom)) {
        return true;
    }

    const std::vector<const Tile*> tiles_at_feet =
        state.stage.GetTilesInRectWc(ToIVec2(feet_tl), ToIVec2(feet_br));
    const bool collided = CollidableTileInList(tiles_at_feet);
    if (collided) {
        return true;
    }
    return false;
}

void DoTileAndEntityCollisions(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Entity& entity = state.entity_manager.entities[entity_idx];

    entity.collided_last_frame = entity.collided;
    entity.collided = false;
    MoveEntityPixelStep(entity_idx, state, true, true, &graphics, &audio);
    entity.collided |= entity.grounded;
}

void DoTileCollisions(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.collided_last_frame = entity.collided;
    entity.collided = false;
    MoveEntityPixelStep(entity_idx, state, true, false, nullptr, nullptr);
}

} // namespace splonks::entities::common
