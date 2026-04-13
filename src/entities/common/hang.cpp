#include "entities/common/common.hpp"

#include "entities/player.hpp"
#include "controls.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHangCountMax = 3;
constexpr std::uint32_t kHangDropCooldownFrames = 5;
constexpr std::uint32_t kHangGloveDropCooldownFrames = 10;
constexpr std::uint32_t kHangWallReleaseCooldownFrames = 4;

bool IsImpassableInRect(const Vec2& tl, const Vec2& br, const State& state, VID self_vid) {
    const AABB area = AABB::New(tl, br);
    const Vec2 anchor = (tl + br) / 2.0F;
    for (const VID& other_vid : QueryEntitiesInAabb(state, area, self_vid)) {
        const Entity* const other = state.entity_manager.GetEntity(other_vid);
        if (other == nullptr || !other->active || !other->impassable) {
            continue;
        }
        if (AabbsIntersect(area, GetNearestWorldAabb(state.stage, anchor, other->GetAABB()))) {
            return true;
        }
    }
    return false;
}

bool IsBlockedForHangProbe(
    const Vec2& tl,
    const Vec2& br,
    const State& state,
    bool check_tiles,
    bool check_entities,
    bool use_hangable_tiles,
    VID self_vid
) {
    if (check_tiles) {
        const IVec2 tl_wc = ToIVec2(tl);
        const IVec2 br_wc = ToIVec2(br);
        if (const std::optional<StageBorderSideKind> tl_side =
                state.stage.GetOutOfBoundsSideForWorldPos(tl_wc)) {
            const Tile border_tile = state.stage.GetBorderTile(*tl_side);
            return use_hangable_tiles ? IsTileHangable(border_tile)
                                      : IsTileCollidable(border_tile);
        }
        if (const std::optional<StageBorderSideKind> br_side =
                state.stage.GetOutOfBoundsSideForWorldPos(br_wc)) {
            const Tile border_tile = state.stage.GetBorderTile(*br_side);
            return use_hangable_tiles ? IsTileHangable(border_tile)
                                      : IsTileCollidable(border_tile);
        }

        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, tl_wc, br_wc)) {
            if (tile_query.tile == nullptr) {
                continue;
            }
            const bool tile_blocked = use_hangable_tiles ? IsTileHangable(*tile_query.tile)
                                                         : IsTileCollidable(*tile_query.tile);
            if (tile_blocked) {
                return true;
            }
        }
    }

    if (check_entities && IsImpassableInRect(tl, br, state, self_vid)) {
        return true;
    }

    return false;
}

bool EntityHasHangGloves(const Entity& entity) {
    if (entity.can_hang_wall) {
        return true;
    }
    if (HasPassiveItem(entity, EntityPassiveItem::Gloves)) {
        return true;
    }
    return false;
}

bool IsTryingToHangOnSide(const Entity& entity, const State& state, bool left_side) {
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    if (left_side) {
        return control.left && !control.right;
    }
    return control.right && !control.left;
}

bool IsSideBlockedForHang(
    const Entity& entity,
    const State& state,
    bool left_side,
    bool check_tiles,
    bool check_entities
) {
    const AABB aabb = entity.GetAABB();
    const Vec2 wall_tl = left_side ? Vec2::New(aabb.tl.x - 1.0F, aabb.tl.y)
                                   : Vec2::New(aabb.br.x, aabb.tl.y);
    const Vec2 wall_br = left_side ? Vec2::New(aabb.tl.x, aabb.br.y)
                                   : Vec2::New(aabb.br.x + 1.0F, aabb.br.y);
    return IsBlockedForHangProbe(
        wall_tl,
        wall_br,
        state,
        check_tiles,
        check_entities,
        true,
        entity.vid
    );
}

bool IsHdHangProbeBlocked(
    const Entity& entity,
    State& state,
    float x,
    float y,
    bool check_tiles,
    bool check_entities,
    bool use_hangable_tiles
) {
    return IsBlockedForHangProbe(
        Vec2::New(x, y),
        Vec2::New(x, y),
        state,
        check_tiles,
        check_entities,
        use_hangable_tiles,
        entity.vid
    );
}

bool CanCornerHangOnSide(
    const Entity& entity,
    State& state,
    bool left_side,
    bool check_tiles,
    bool check_entities
) {
    const AABB aabb = entity.GetAABB();
    const float side_x = left_side ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
    const float upper_probe_y_a = aabb.tl.y + 2.0F;
    const float upper_probe_y_b = aabb.tl.y + 3.0F;
    const float center_x = aabb.tl.x + std::floor(entity.size.x / 2.0F);
    const float below_probe_y = aabb.br.y + 1.0F;

    const bool upper_probe_blocked =
        IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities, true) ||
        IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities, true);
    const bool above_probe_blocked =
        IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities, false);
    const bool below_probe_blocked =
        IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities, false);

    return upper_probe_blocked && !above_probe_blocked && !below_probe_blocked;
}

bool CanGloveHangBelowCorner(
    const Entity& entity,
    State& state,
    bool left_side,
    bool check_tiles,
    bool check_entities
) {
    const AABB aabb = entity.GetAABB();
    const float side_x = left_side ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
    const int start_y = static_cast<int>(std::floor(aabb.tl.y)) - 1;
    const int end_y = static_cast<int>(std::floor(aabb.br.y));

    for (int y = start_y; y <= end_y; ++y) {
        if (IsHdHangProbeBlocked(
                entity,
                state,
                side_x,
                static_cast<float>(y),
                check_tiles,
                check_entities,
                true
            )) {
            return aabb.tl.y >= static_cast<float>(y);
        }
    }

    return false;
}

bool TryCaptureHdHang(
    std::size_t entity_idx,
    State& state,
    bool check_tiles,
    bool check_entities
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.can_hang_ledge && !EntityHasHangGloves(entity)) {
        return false;
    }
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    if (control.no_hang || entity.hang_count > 0) {
        return false;
    }
    if (entity.condition != EntityCondition::Normal) {
        return false;
    }
    if (entity.vel.y <= 0.0F) {
        return false;
    }
    if (entity.grounded || entity.IsClimbing() || entity.IsHanging()) {
        return false;
    }

    const bool try_left = IsTryingToHangOnSide(entity, state, true);
    const bool try_right = IsTryingToHangOnSide(entity, state, false);
    if (!try_left && !try_right) {
        return false;
    }

    const AABB aabb = entity.GetAABB();
    const bool top_blocked = IsBlockedForHangProbe(
        Vec2::New(aabb.tl.x, aabb.tl.y - 1.0F),
        Vec2::New(aabb.br.x, aabb.tl.y - 1.0F),
        state,
        check_tiles,
        check_entities,
        false,
        entity.vid
    );
    if (top_blocked) {
        return false;
    }

    const bool has_gloves = EntityHasHangGloves(entity);
    const float center_x = aabb.tl.x + std::floor(entity.size.x / 2.0F);
    const float upper_probe_y_a = aabb.tl.y + 2.0F;
    const float upper_probe_y_b = aabb.tl.y + 3.0F;
    const float below_probe_y = aabb.br.y + 1.0F;

    if (try_left && IsSideBlockedForHang(entity, state, true, check_tiles, check_entities)) {
        const float side_x = aabb.tl.x - 1.0F;
        if (has_gloves) {
            if (CanCornerHangOnSide(entity, state, true, check_tiles, check_entities)) {
                entity.pos.y =
                    std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
                entity.hang_side = LeftOrRight::Left;
                SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
                entity.facing = LeftOrRight::Left;
                entity.vel.y = 0.0F;
                entity.acc.y = 0.0F;
                entity.grounded = false;
                return true;
            }
            if (!CanGloveHangBelowCorner(entity, state, true, check_tiles, check_entities)) {
                return false;
            }
            entity.hang_side = LeftOrRight::Left;
            SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
            entity.facing = LeftOrRight::Left;
            entity.vel.y = 0.0F;
            entity.acc.y = 0.0F;
            entity.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities, true) ||
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities, true);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities, false);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities, false);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        entity.pos.y =
            std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
        entity.hang_side = LeftOrRight::Left;
        SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
        entity.facing = LeftOrRight::Left;
        entity.vel.y = 0.0F;
        entity.acc.y = 0.0F;
        entity.grounded = false;
        return true;
    }

    if (try_right && IsSideBlockedForHang(entity, state, false, check_tiles, check_entities)) {
        const float side_x = aabb.br.x + 1.0F;
        if (has_gloves) {
            if (CanCornerHangOnSide(entity, state, false, check_tiles, check_entities)) {
                entity.pos.y =
                    std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
                entity.hang_side = LeftOrRight::Right;
                SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
                entity.facing = LeftOrRight::Right;
                entity.vel.y = 0.0F;
                entity.acc.y = 0.0F;
                entity.grounded = false;
                return true;
            }
            if (!CanGloveHangBelowCorner(entity, state, false, check_tiles, check_entities)) {
                return false;
            }
            entity.hang_side = LeftOrRight::Right;
            SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
            entity.facing = LeftOrRight::Right;
            entity.vel.y = 0.0F;
            entity.acc.y = 0.0F;
            entity.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities, true) ||
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities, true);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities, false);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities, false);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        entity.pos.y =
            std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
        entity.hang_side = LeftOrRight::Right;
        SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
        entity.facing = LeftOrRight::Right;
        entity.vel.y = 0.0F;
        entity.acc.y = 0.0F;
        entity.grounded = false;
        return true;
    }

    return false;
}

} // namespace

void HangHandsStep(std::size_t entity_idx, State& state) {
    Entity& mutable_entity = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(mutable_entity, state);
    if (control.no_hang) {
        mutable_entity.hang_side.reset();
        SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
    }
    if (mutable_entity.condition != EntityCondition::Normal) {
        mutable_entity.hang_side.reset();
        SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
    }

    if (mutable_entity.hang_side == LeftOrRight::Left) {
        const bool has_gloves = EntityHasHangGloves(mutable_entity);
        const bool still_trying = IsTryingToHangOnSide(mutable_entity, state, true);
        if ((has_gloves && !still_trying) ||
            !IsSideBlockedForHang(mutable_entity, state, true, true, true)) {
            mutable_entity.hang_side.reset();
            SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
            mutable_entity.hang_count = kHangWallReleaseCooldownFrames;
        }
    } else if (mutable_entity.hang_side == LeftOrRight::Right) {
        const bool has_gloves = EntityHasHangGloves(mutable_entity);
        const bool still_trying = IsTryingToHangOnSide(mutable_entity, state, false);
        if ((has_gloves && !still_trying) ||
            !IsSideBlockedForHang(mutable_entity, state, false, true, true)) {
            mutable_entity.hang_side.reset();
            SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
            mutable_entity.hang_count = kHangWallReleaseCooldownFrames;
        }
    }

    if (!mutable_entity.IsHanging()) {
        TryCaptureHdHang(entity_idx, state, true, true);
    }

    if (mutable_entity.IsHanging()) {
        mutable_entity.vel.y = 0.0F;
        mutable_entity.acc.y = 0.0F;
        mutable_entity.grounded = false;
        mutable_entity.coyote_time = player::kCoyoteTimeFrames;
    }
}

void JumpingAndClimbingStep(std::size_t entity_idx, State& state, Audio& audio) {
    GroundedCheck(entity_idx, state, audio, true, true);

    Entity& entity = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    Stage& stage = state.stage;
    const auto [player_tl, player_br] = entity.GetBounds();
    bool can_climb = false;
    for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
             stage,
             ToIVec2(player_tl),
             ToIVec2(player_br))) {
        if (tile_query.tile != nullptr && GetTileArchetype(*tile_query.tile).climbable) {
            can_climb = true;
            break;
        }
    }

    if (can_climb) {
        if (control.up) {
            SetMovementFlag(entity, EntityMovementFlag::Climbing, true);
            entity.grounded = false;
            entity.vel.y = -player::kClimbSpeed;
        } else if (entity.IsClimbing() && !control.down) {
            entity.vel.y = 0.0F;
        }
        if (entity.IsClimbing() && !control.up && control.down) {
            entity.vel.y = player::kClimbSpeed;
        }
        if (entity.IsClimbing() && control.jump_pressed && control.down) {
            SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
        }
    } else {
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
    }

    if (entity.IsClimbing() && entity.grounded) {
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
    }

    if (entity.condition != EntityCondition::Normal) {
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
    }

    if (control.jump_pressed) {
        if (entity.IsHanging()) {
            const bool jumping_away =
                (entity.hang_side == LeftOrRight::Right && control.left) ||
                (entity.hang_side == LeftOrRight::Left && control.right);
            const bool has_gloves = EntityHasHangGloves(entity);
            entity.hang_side.reset();
            SetMovementFlag(entity, EntityMovementFlag::Hanging, false);
            entity.grounded = false;
            if (control.down) {
                entity.hang_count =
                    has_gloves ? kHangGloveDropCooldownFrames : kHangDropCooldownFrames;
            } else if (jumping_away) {
                entity.hang_count = kHangCountMax;
            } else {
                entity.vel.y = -player::kJumpImpulse;
                entity.hang_count = kHangCountMax;
            }
        } else if ((entity.grounded && (entity.jump_delay_frame_count == 0)) || entity.coyote_time > 0) {
            entity.jumped_this_frame = true;
            entity.vel.y = -player::kJumpImpulse;
            entity.coyote_time = 0;
            entity.grounded = false;
            entity.jump_delay_frame_count = player::kJumpDelayFrames;
            audio.PlaySoundEffect(SoundEffect::Jump);
        }
    }

    if (entity.jump_delay_frame_count > 0) {
        entity.jump_delay_frame_count -= 1;
    }

    if (!entity.IsClimbing() && !entity.grounded && !entity.IsHanging()) {
        entity.acc.y += state.stage.gravity;
    }
}

} // namespace splonks::entities::common
