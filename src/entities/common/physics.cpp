#include "entities/common/common.hpp"
#include "world_query.hpp"

#include "entity/archetype.hpp"
#include "entities/player.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

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

int FloorDiv(int value, int divisor) {
    const int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        return quotient - 1;
    }
    return quotient;
}

float GetGroundFrictionMultiplier(std::size_t entity_idx, State& state) {
    constexpr float kDefaultGroundFriction = 0.85F;
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const auto [entity_tl, entity_br] = entity.GetBounds();
    const int support_y = static_cast<int>(std::floor(entity_br.y + 1.0F));
    const int min_tile_x = FloorDiv(static_cast<int>(std::floor(entity_tl.x)), static_cast<int>(kTileSize));
    const int max_tile_x = FloorDiv(static_cast<int>(std::floor(entity_br.x)), static_cast<int>(kTileSize));
    const int support_tile_y = FloorDiv(support_y, static_cast<int>(kTileSize));

    float friction = 0.0F;
    bool found_support_surface = false;
    for (int tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
        const Tile tile = state.stage.GetTileOrBorder(tile_x, support_tile_y);
        if (!IsTileCollidable(tile)) {
            continue;
        }
        const float tile_friction = GetTileFriction(tile);
        friction = found_support_surface ? std::min(friction, tile_friction) : tile_friction;
        found_support_surface = true;
    }

    const VID vid = entity.vid;
    const AABB feet_aabb = {
        .tl = Vec2::New(entity_tl.x, entity_br.y),
        .br = entity_br + Vec2::New(0.0F, 1.0F),
    };
    for (const VID& other_vid : QueryEntitiesInAabb(state, feet_aabb, vid)) {
        const Entity* const other = state.entity_manager.GetEntity(other_vid);
        if (other == nullptr || !other->active || !other->impassable) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(state.stage, entity.GetCenter(), other->GetAABB());
        if (!AabbsIntersect(feet_aabb, other_aabb)) {
            continue;
        }

        friction = found_support_surface
                       ? std::min(friction, other->support_ground_friction)
                       : other->support_ground_friction;
        found_support_surface = true;
    }

    return found_support_surface ? friction : kDefaultGroundFriction;
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

AABB GetTopCarryStrip(const Entity& entity) {
    const AABB aabb = entity.GetAABB();
    return AABB{
        .tl = Vec2::New(aabb.tl.x, aabb.tl.y - 1.0F),
        .br = Vec2::New(aabb.br.x, aabb.tl.y - 1.0F),
    };
}

AABB GetTopCarryQueryArea(const Entity& entity, const IVec2& direction) {
    const AABB carry_strip = GetTopCarryStrip(entity);
    if (direction.y > 0) {
        return AABB{
            .tl = Vec2::New(carry_strip.tl.x, carry_strip.tl.y - 1.0F),
            .br = carry_strip.br,
        };
    }
    return carry_strip;
}

bool IsCarryTargetOnTopOfMover(
    const Entity& mover,
    const Entity& target,
    const Stage& stage
) {
    if (!target.active || !target.can_collide || target.impassable) {
        return false;
    }
    if (target.held_by_vid.has_value() || target.attachment_mode != AttachmentMode::None) {
        return false;
    }

    const AABB carry_strip = GetTopCarryStrip(mover);
    const AABB target_feet = GetNearestWorldAabb(stage, mover.GetCenter(), target.GetFeet());
    if (!AabbsIntersect(carry_strip, target_feet)) {
        return false;
    }

    const float overlap_x =
        std::min(carry_strip.br.x, target_feet.br.x) -
        std::max(carry_strip.tl.x, target_feet.tl.x);
    return overlap_x > 0.0F;
}

AABB GetHangCarryStripForMoverSide(const Entity& mover, LeftOrRight mover_side) {
    const AABB aabb = mover.GetAABB();
    if (mover_side == LeftOrRight::Right) {
        return AABB{
            .tl = Vec2::New(aabb.br.x + 1.0F, aabb.tl.y),
            .br = Vec2::New(aabb.br.x + 1.0F, aabb.br.y),
        };
    }
    return AABB{
        .tl = Vec2::New(aabb.tl.x - 1.0F, aabb.tl.y),
        .br = Vec2::New(aabb.tl.x - 1.0F, aabb.br.y),
    };
}

bool IsHangCarryTargetOnMoverSide(
    const Entity& mover,
    const Entity& target,
    const Stage& stage,
    LeftOrRight mover_side
) {
    if (!target.active || !target.can_collide || target.impassable || !target.IsHanging()) {
        return false;
    }
    if (target.held_by_vid.has_value() || target.attachment_mode != AttachmentMode::None) {
        return false;
    }

    if (mover_side == LeftOrRight::Left) {
        if (target.hang_side != LeftOrRight::Right) {
            return false;
        }
    } else {
        if (target.hang_side != LeftOrRight::Left) {
            return false;
        }
    }

    const AABB mover_aabb = mover.GetAABB();
    const AABB target_aabb = GetNearestWorldAabb(stage, mover.GetCenter(), target.GetAABB());
    const float overlap_y =
        std::min(mover_aabb.br.y, target_aabb.br.y) -
        std::max(mover_aabb.tl.y, target_aabb.tl.y);
    if (overlap_y <= 0.0F) {
        return false;
    }

    if (mover_side == LeftOrRight::Right) {
        return target_aabb.tl.x == mover_aabb.br.x + 1.0F;
    }
    return target_aabb.br.x == mover_aabb.tl.x - 1.0F;
}

void AppendHangCarryTargetsOnMoverSide(
    std::size_t mover_idx,
    LeftOrRight mover_side,
    State& state,
    std::vector<VID>& hanger_vids
) {
    if (mover_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& mover = state.entity_manager.entities[mover_idx];
    if (!mover.active || !mover.impassable) {
        return;
    }

    for (const VID& vid : QueryEntitiesInAabb(
             state,
             GetHangCarryStripForMoverSide(mover, mover_side),
             mover.vid)) {
        const Entity* const target = state.entity_manager.GetEntity(vid);
        if (target == nullptr) {
            continue;
        }
        if (!IsHangCarryTargetOnMoverSide(mover, *target, state.stage, mover_side)) {
            continue;
        }
        if (std::find(hanger_vids.begin(), hanger_vids.end(), vid) == hanger_vids.end()) {
            hanger_vids.push_back(vid);
        }
    }
}

void TryCarryEntitiesOnTopByOnePixel(
    std::size_t mover_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
) {
    if (direction.x == 0 && direction.y <= 0) {
        return;
    }
    if (mover_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& mover = state.entity_manager.entities[mover_idx];
    if (!mover.active || !mover.impassable) {
        return;
    }

    std::vector<VID> rider_vids;
    for (const VID& vid : QueryEntitiesInAabb(
             state,
             GetTopCarryQueryArea(mover, direction),
             mover.vid)) {
        const Entity* const target = state.entity_manager.GetEntity(vid);
        if (target == nullptr) {
            continue;
        }
        if (!IsCarryTargetOnTopOfMover(mover, *target, state.stage)) {
            continue;
        }
        rider_vids.push_back(vid);
    }

    std::sort(
        rider_vids.begin(),
        rider_vids.end(),
        [&](const VID& lhs, const VID& rhs) {
            const Entity* const left = state.entity_manager.GetEntity(lhs);
            const Entity* const right = state.entity_manager.GetEntity(rhs);
            if (left == nullptr || right == nullptr) {
                return false;
            }

            const float left_x =
                GetNearestWorldAabb(state.stage, mover.GetCenter(), left->GetAABB()).tl.x;
            const float right_x =
                GetNearestWorldAabb(state.stage, mover.GetCenter(), right->GetAABB()).tl.x;
            if (direction.x > 0) {
                return left_x > right_x;
            }
            if (direction.x < 0) {
                return left_x < right_x;
            }
            return false;
        }
    );

    for (const VID& rider_vid : rider_vids) {
        if (!TryDisplaceEntityByOnePixel(rider_vid.id, direction, state, graphics, audio)) {
            continue;
        }
        SyncEntityAttachments(rider_vid.id, state, graphics);
    }
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
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Left,
                state,
                hanging_carry_vids
            );
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
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntityByOnePixel(
                            hanging_vid.id,
                            IVec2::New(1, 0),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntityAttachments(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntitiesOnTopByOnePixel(
                    entity_idx,
                    IVec2::New(1, 0),
                    state,
                    *graphics,
                    audio
                );
            }
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
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Right,
                state,
                hanging_carry_vids
            );
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
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntityByOnePixel(
                            hanging_vid.id,
                            IVec2::New(-1, 0),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntityAttachments(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntitiesOnTopByOnePixel(
                    entity_idx,
                    IVec2::New(-1, 0),
                    state,
                    *graphics,
                    audio
                );
            }
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
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Left,
                state,
                hanging_carry_vids
            );
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Right,
                state,
                hanging_carry_vids
            );
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
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntityByOnePixel(
                            hanging_vid.id,
                            IVec2::New(0, 1),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntityAttachments(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntitiesOnTopByOnePixel(
                    entity_idx,
                    IVec2::New(0, 1),
                    state,
                    *graphics,
                    audio
                );
            }
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
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Left,
                state,
                hanging_carry_vids
            );
            AppendHangCarryTargetsOnMoverSide(
                entity_idx,
                LeftOrRight::Right,
                state,
                hanging_carry_vids
            );
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
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntityByOnePixel(
                            hanging_vid.id,
                            IVec2::New(0, -1),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntityAttachments(hanging_vid.id, state, *graphics);
                }
            }
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
        QueryEntitiesInAabb(state, feet_aabb, vid);

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
    entity.vel.x = std::clamp(entity.vel.x, -entity.max_speed, entity.max_speed);
    entity.vel.y = std::clamp(entity.vel.y, -entity.max_speed, entity.max_speed);
    entity.acc = Vec2::New(0.0F, 0.0F);
}

void ApplyGroundFriction(std::size_t entity_idx, State& state) {
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
        entity.vel.x *= GetGroundFrictionMultiplier(entity_idx, state);
    }
}

void ApplyArchetypeGroundFriction(std::size_t entity_idx, State& state) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.affected_by_ground_friction) {
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

    for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
             state.stage,
             ToIVec2(feet_tl),
             ToIVec2(feet_br))) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return true;
        }
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
