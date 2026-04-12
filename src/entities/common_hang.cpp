#include "entities/common.hpp"

#include "entities/player.hpp"
#include "systems/controls.hpp"
#include "tile.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHangCountMax = 3;
constexpr std::uint32_t kHangDropCooldownFrames = 5;
constexpr std::uint32_t kHangGloveDropCooldownFrames = 10;
constexpr std::uint32_t kHangWallReleaseCooldownFrames = 4;

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

bool IsImpassableInRect(const Vec2& tl, const Vec2& br, const State& state, VID self_vid) {
    for (const Entity& other : state.entity_manager.entities) {
        if (!other.active || other.vid == self_vid || !other.impassable) {
            continue;
        }
        if (AabbsIntersect(AABB::New(tl, br), other.GetAABB())) {
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
    VID self_vid
) {
    if (check_tiles) {
        if (state.settings.post_process.terrain_face_enclosed_stage_bounds) {
            if (tl.x < 0.0F || br.x >= static_cast<float>(state.stage.GetWidth())) {
                return true;
            }
        }

        const std::vector<const Tile*> tiles = state.stage.GetTilesInRectWc(ToIVec2(tl), ToIVec2(br));
        if (CollidableTileInList(tiles)) {
            return true;
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
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(entity, state);
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
        entity.vid
    );
}

bool IsHdHangProbeBlocked(
    const Entity& entity,
    State& state,
    float x,
    float y,
    bool check_tiles,
    bool check_entities
) {
    return IsBlockedForHangProbe(
        Vec2::New(x, y),
        Vec2::New(x, y),
        state,
        check_tiles,
        check_entities,
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
        IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities) ||
        IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities);
    const bool above_probe_blocked =
        IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities);
    const bool below_probe_blocked =
        IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities);

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
                check_entities
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
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(entity, state);
    if (control.no_hang || entity.hang_count > 0) {
        return false;
    }
    if (entity.condition != EntityCondition::Normal) {
        return false;
    }
    if (entity.vel.y <= 0.0F) {
        return false;
    }
    if (entity.grounded || entity.climbing || entity.IsHanging()) {
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
                entity.left_hanging = true;
                entity.right_hanging = false;
                entity.facing = LeftOrRight::Left;
                entity.vel.y = 0.0F;
                entity.acc.y = 0.0F;
                entity.grounded = false;
                return true;
            }
            if (!CanGloveHangBelowCorner(entity, state, true, check_tiles, check_entities)) {
                return false;
            }
            entity.left_hanging = true;
            entity.right_hanging = false;
            entity.facing = LeftOrRight::Left;
            entity.vel.y = 0.0F;
            entity.acc.y = 0.0F;
            entity.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities) ||
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        entity.pos.y =
            std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
        entity.left_hanging = true;
        entity.right_hanging = false;
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
                entity.left_hanging = false;
                entity.right_hanging = true;
                entity.facing = LeftOrRight::Right;
                entity.vel.y = 0.0F;
                entity.acc.y = 0.0F;
                entity.grounded = false;
                return true;
            }
            if (!CanGloveHangBelowCorner(entity, state, false, check_tiles, check_entities)) {
                return false;
            }
            entity.left_hanging = false;
            entity.right_hanging = true;
            entity.facing = LeftOrRight::Right;
            entity.vel.y = 0.0F;
            entity.acc.y = 0.0F;
            entity.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_a, check_tiles, check_entities) ||
            IsHdHangProbeBlocked(entity, state, side_x, upper_probe_y_b, check_tiles, check_entities);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(entity, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_entities);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(entity, state, center_x, below_probe_y, check_tiles, check_entities);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        entity.pos.y =
            std::round(entity.pos.y / static_cast<float>(kTileSize)) * static_cast<float>(kTileSize);
        entity.left_hanging = false;
        entity.right_hanging = true;
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
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(mutable_entity, state);
    if (control.no_hang) {
        mutable_entity.left_hanging = false;
        mutable_entity.right_hanging = false;
    }
    if (mutable_entity.condition != EntityCondition::Normal) {
        mutable_entity.left_hanging = false;
        mutable_entity.right_hanging = false;
    }

    if (mutable_entity.left_hanging) {
        const bool has_gloves = EntityHasHangGloves(mutable_entity);
        const bool still_trying = IsTryingToHangOnSide(mutable_entity, state, true);
        if ((has_gloves && !still_trying) ||
            !IsSideBlockedForHang(mutable_entity, state, true, true, true)) {
            mutable_entity.left_hanging = false;
            mutable_entity.hang_count = kHangWallReleaseCooldownFrames;
        }
    } else if (mutable_entity.right_hanging) {
        const bool has_gloves = EntityHasHangGloves(mutable_entity);
        const bool still_trying = IsTryingToHangOnSide(mutable_entity, state, false);
        if ((has_gloves && !still_trying) ||
            !IsSideBlockedForHang(mutable_entity, state, false, true, true)) {
            mutable_entity.right_hanging = false;
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
    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(entity, state);
    Stage& stage = state.stage;
    const auto [player_tl, player_br] = entity.GetBounds();
    const std::vector<const Tile*> newly_collided_tiles =
        stage.GetTilesInRectWc(ToIVec2(player_tl), ToIVec2(player_br));
    const bool can_climb = ClimbableTileInList(newly_collided_tiles);

    if (can_climb) {
        if (control.up) {
            entity.climbing = true;
            entity.grounded = false;
            entity.vel.y = -player::kClimbSpeed;
        } else if (entity.climbing && !control.down) {
            entity.vel.y = 0.0F;
        }
        if (entity.climbing && !control.up && control.down) {
            entity.vel.y = player::kClimbSpeed;
        }
        if (entity.climbing && control.jump_pressed && control.down) {
            entity.climbing = false;
        }
    } else {
        entity.climbing = false;
    }

    if (entity.climbing && entity.grounded) {
        entity.climbing = false;
    }

    if (entity.condition != EntityCondition::Normal) {
        entity.climbing = false;
    }

    if (control.jump_pressed) {
        if (entity.IsHanging()) {
            const bool jumping_away =
                (entity.right_hanging && control.left) ||
                (entity.left_hanging && control.right);
            const bool has_gloves = EntityHasHangGloves(entity);
            entity.left_hanging = false;
            entity.right_hanging = false;
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

    if (!entity.climbing && !entity.grounded && !entity.IsHanging()) {
        entity.acc.y += state.stage.gravity;
    }
}

} // namespace splonks::entities::common
