#include "entities/common.hpp"

#include "entities/baseball_bat.hpp"
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

bool AabbsIntersect(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

bool IsBlockedByStageBounds(const AABB& aabb, const Stage& stage) {
    if (aabb.tl.x < 0.0F || aabb.tl.y < 0.0F) {
        return true;
    }
    if (aabb.br.x > static_cast<float>(stage.GetWidth() - 1)) {
        return true;
    }
    if (aabb.br.y > static_cast<float>(stage.GetHeight() - 1)) {
        return true;
    }
    return false;
}

bool IsBlockedByTiles(const AABB& aabb, const Stage& stage) {
    const std::vector<const Tile*> collided_tiles =
        stage.GetTilesInRectWc(ToIVec2(aabb.tl), ToIVec2(aabb.br));
    return CollidableTileInList(collided_tiles);
}

bool IsBlockedByImpassableEntities(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state
) {
    const VID self_vid = state.entity_manager.entities[entity_idx].vid;
    for (const Entity& other : state.entity_manager.entities) {
        if (!other.active) {
            continue;
        }
        if (other.vid.id == self_vid.id) {
            continue;
        }
        if (!other.impassable) {
            continue;
        }
        if (AabbsIntersect(aabb, other.GetAABB())) {
            return true;
        }
    }
    return false;
}

bool IsBlockingAabbForEntity(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state,
    bool check_tiles,
    bool check_entities
) {
    if (check_tiles) {
        if (IsBlockedByStageBounds(aabb, state.stage)) {
            return true;
        }
        if (IsBlockedByTiles(aabb, state.stage)) {
            return true;
        }
    }

    if (check_entities && IsBlockedByImpassableEntities(entity_idx, aabb, state)) {
        return true;
    }

    return false;
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
    if (!IsBlockingAabbForEntity(entity_idx, current_aabb, state, check_tiles, check_entities)) {
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
            if (!IsBlockingAabbForEntity(
                    entity_idx,
                    candidate_aabb,
                    state,
                    check_tiles,
                    check_entities
                )) {
                entity.pos = candidate_pos;
                return;
            }
        }
    }
}

bool ApplySweptContactInteractions(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active) {
        return false;
    }

    for (std::size_t other_entity_idx = 0; other_entity_idx < state.entity_manager.entities.size();
         ++other_entity_idx) {
        if (other_entity_idx == entity_idx) {
            continue;
        }
        const Entity& other_entity = state.entity_manager.entities[other_entity_idx];
        if (!other_entity.active) {
            continue;
        }

        if (entity.type_ == EntityType::BaseballBat) {
            if (baseball_bat::TryApplyBatContactToEntity(
                    entity_idx,
                    other_entity_idx,
                    state,
                    graphics,
                    audio
                )) {
                return true;
            }
        }
        if (other_entity.type_ == EntityType::BaseballBat) {
            if (baseball_bat::TryApplyBatContactToEntity(
                    other_entity_idx,
                    entity_idx,
                    state,
                    graphics,
                    audio
                )) {
                return true;
            }
        }
    }

    return false;
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
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (graphics != nullptr && audio != nullptr) {
                if (ApplySweptContactInteractions(entity_idx, state, *graphics, *audio)) {
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
            }
        }
    } else if (move_x < 0) {
        for (int i = 0; i < -move_x; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(-1.0F, 0.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (graphics != nullptr && audio != nullptr) {
                if (ApplySweptContactInteractions(entity_idx, state, *graphics, *audio)) {
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
            }
        }
    }

    if (move_y > 0) {
        for (int i = 0; i < move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, 1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (graphics != nullptr && audio != nullptr) {
                if (ApplySweptContactInteractions(entity_idx, state, *graphics, *audio)) {
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
            }
        }
    } else if (move_y < 0) {
        for (int i = 0; i < -move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, -1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
            if (graphics != nullptr && audio != nullptr) {
                if (ApplySweptContactInteractions(entity_idx, state, *graphics, *audio)) {
                    StoreDistanceTraveled(entity_idx, state, start_pos);
                    return;
                }
            }
        }
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
    if (feet_br.y >= static_cast<float>(state.stage.GetHeight())) {
        return true;
    }

    const IVec2 feet_tl_tile_pos = ToIVec2(feet_tl) / static_cast<int>(kTileSize);
    const IVec2 feet_br_tile_pos = ToIVec2(feet_br) / static_cast<int>(kTileSize);
    const std::vector<const Tile*> tiles_at_feet =
        state.stage.GetTilesInRect(feet_tl_tile_pos, feet_br_tile_pos);
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
