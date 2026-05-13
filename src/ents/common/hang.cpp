#include "entities/common/common.hpp"

#include "controls.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHangCountMax = 3;
constexpr std::uint32_t kHangCoyoteTimeFrames = 6;
constexpr int kGroundedDownSideAttachRequiredProbeHits = 2;
constexpr float kSpringShoeMovementSoundVolume = 0.15F;
constexpr float kLocomotionClaimMaxDistancePx = 32.0F;
constexpr float kLocomotionClaimUpwardVelocityGrace = -0.5F;
constexpr float kLocomotionClaimMaxHorizontalVelocityPx = 12.0F;
constexpr float kLocomotionClaimMaxVerticalVelocityPx = 16.0F;

struct ClimbAnchor {
    IVec2 tile_pos = IVec2::New(0, 0);
};

struct ClimbProbePoints {
    Vec2 left = Vec2::New(0.0F, 0.0F);
    Vec2 center = Vec2::New(0.0F, 0.0F);
    Vec2 right = Vec2::New(0.0F, 0.0F);
};

bool CanAttachDownToClimbAnchor(const ClimbAnchor& climb_anchor, const State& state);

int GetRequiredClimbProbeHits(const JumpAndClimbTuning& tuning) {
    return static_cast<int>(std::clamp<std::uint32_t>(tuning.climb_required_probe_hits, 1, 3));
}

void AddClimbDebugLabel(State& state, const Vec2& world_pos, const char* text) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = world_pos,
        .text = text,
        .color = DebugAnnotationColor{255, 240, 64, 255},
    });
}

void AddClimbDebugRect(State& state, const Vec2& world_pos, DebugAnnotationColor color) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = AABB::New(world_pos, world_pos),
        .color = color,
    });
}

ClimbProbePoints GetClimbProbePointsAtPosition(
    const Entity& entity,
    const Vec2& pos,
    const JumpAndClimbTuning& tuning
) {
    const Vec2 center = pos + (entity.size / 2.0F);
    const float probe_y =
        pos.y + std::min(tuning.climb_probe_bias_pixels, std::max(0.0F, entity.size.y - 1.0F));
    const float horizontal_offset = (entity.size.x * 0.5F) * std::max(0.0F, tuning.climb_probe_x_scale);
    return ClimbProbePoints{
        .left = Vec2::New(center.x - horizontal_offset, probe_y),
        .center = Vec2::New(center.x, probe_y),
        .right = Vec2::New(center.x + horizontal_offset, probe_y),
    };
}

bool IsClimbableTileQuery(
    const Stage& stage,
    const std::optional<WorldTileQueryResult>& tile_query
) {
    return tile_query.has_value() && IsTileQueryClimbable(stage, *tile_query);
}

std::optional<ClimbAnchor> GetClimbAnchorAtPosition(
    const Entity& entity,
    const Vec2& pos,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(entity, pos, tuning);
    const std::array<IVec2, 3> probe_points = {
        ToIVec2(probes.left),
        ToIVec2(probes.center),
        ToIVec2(probes.right),
    };

    std::optional<IVec2> best_tile = std::nullopt;
    int best_hits = 0;
    float best_score = 0.0F;
    const Vec2 entity_center = pos + (entity.size / 2.0F);

    for (const IVec2& probe_point : probe_points) {
        const std::optional<WorldTileQueryResult> tile_query =
            QueryTileAtWorldPos(state.stage, probe_point);
        if (!IsClimbableTileQuery(state.stage, tile_query)) {
            continue;
        }

        int hits = 0;
        for (const IVec2& candidate_probe_point : probe_points) {
            const std::optional<WorldTileQueryResult> candidate_query =
                QueryTileAtWorldPos(state.stage, candidate_probe_point);
            if (!IsClimbableTileQuery(state.stage, candidate_query)) {
                continue;
            }
            if (candidate_query->tile_pos == tile_query->tile_pos) {
                hits += 1;
            }
        }

        if (hits < GetRequiredClimbProbeHits(tuning)) {
            continue;
        }

        const Vec2 tile_center = Vec2::New(
            static_cast<float>(tile_query->tile_pos.x * static_cast<int>(kTileSize) + 8),
            static_cast<float>(tile_query->tile_pos.y * static_cast<int>(kTileSize) + 8)
        );
        const float dx = std::abs(tile_center.x - entity_center.x);
        const float dy = std::abs(tile_center.y - entity_center.y);
        const float score = dx + (dy * 0.25F);
        if (!best_tile.has_value() || hits > best_hits || (hits == best_hits && score < best_score)) {
            best_tile = tile_query->tile_pos;
            best_hits = hits;
            best_score = score;
        }
    }

    if (!best_tile.has_value()) {
        return std::nullopt;
    }

    return ClimbAnchor{.tile_pos = *best_tile};
}

std::optional<ClimbAnchor> GetClimbAnchorFromProbePoints(
    const Entity& entity,
    const State& state,
    const JumpAndClimbTuning& tuning,
    const std::array<IVec2, 3>& probe_points
) {
    std::optional<IVec2> best_tile = std::nullopt;
    int best_hits = 0;
    float best_score = 0.0F;
    const Vec2 entity_center = entity.GetCenter();

    for (const IVec2& probe_point : probe_points) {
        const std::optional<WorldTileQueryResult> tile_query =
            QueryTileAtWorldPos(state.stage, probe_point);
        if (!IsClimbableTileQuery(state.stage, tile_query)) {
            continue;
        }

        int hits = 0;
        for (const IVec2& candidate_probe_point : probe_points) {
            const std::optional<WorldTileQueryResult> candidate_query =
                QueryTileAtWorldPos(state.stage, candidate_probe_point);
            if (IsClimbableTileQuery(state.stage, candidate_query) &&
                candidate_query->tile_pos == tile_query->tile_pos) {
                hits += 1;
            }
        }

        if (hits < GetRequiredClimbProbeHits(tuning)) {
            continue;
        }

        const Vec2 tile_center = Vec2::New(
            static_cast<float>(tile_query->tile_pos.x * static_cast<int>(kTileSize) + 8),
            static_cast<float>(tile_query->tile_pos.y * static_cast<int>(kTileSize) + 8)
        );
        const float dx = std::abs(tile_center.x - entity_center.x);
        const float dy = std::abs(tile_center.y - entity_center.y);
        const float score = dx + (dy * 0.25F);
        if (!best_tile.has_value() || hits > best_hits || (hits == best_hits && score < best_score)) {
            best_tile = tile_query->tile_pos;
            best_hits = hits;
            best_score = score;
        }
    }

    if (!best_tile.has_value()) {
        return std::nullopt;
    }

    return ClimbAnchor{.tile_pos = *best_tile};
}

std::optional<ClimbAnchor> GetClimbAnchor(
    const Entity& entity,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    return GetClimbAnchorAtPosition(entity, entity.pos, state, tuning);
}

std::optional<ClimbAnchor> GetGroundedDownClimbAnchor(
    const Entity& entity,
    State& state,
    const JumpAndClimbTuning& tuning
) {
    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(entity, entity.pos, tuning);
    const AABB aabb = entity.GetAABB();
    const std::array<IVec2, 3> normal_probe_points = {
        ToIVec2(probes.left),
        ToIVec2(probes.center),
        ToIVec2(probes.right),
    };
    const int probe_y = static_cast<int>(std::floor(aabb.br.y + 1.0F));
    const std::array<IVec2, 3> probe_points = {
        IVec2::New(static_cast<int>(std::floor(probes.left.x)), probe_y),
        IVec2::New(static_cast<int>(std::floor(probes.center.x)), probe_y),
        IVec2::New(static_cast<int>(std::floor(probes.right.x)), probe_y),
    };

    std::optional<ClimbAnchor> climb_anchor =
        GetClimbAnchorFromProbePoints(entity, state, tuning, probe_points);
    if (!climb_anchor.has_value() || !CanAttachDownToClimbAnchor(*climb_anchor, state)) {
        climb_anchor = std::nullopt;
    }
    if (climb_anchor.has_value()) {
        return climb_anchor;
    }

    const std::array<IVec2, 2> edge_probe_points = {
        IVec2::New(static_cast<int>(std::floor(aabb.tl.x - 1.0F)), probe_y),
        IVec2::New(static_cast<int>(std::floor(aabb.br.x + 1.0F)), probe_y),
    };
    for (const IVec2& edge_probe_point : edge_probe_points) {
        AddClimbDebugRect(state, ToVec2(edge_probe_point), DebugAnnotationColor{255, 240, 64, 255});
        const std::optional<WorldTileQueryResult> edge_query =
            QueryTileAtWorldPos(state.stage, edge_probe_point);
        if (!IsClimbableTileQuery(state.stage, edge_query)) {
            AddClimbDebugLabel(state, ToVec2(edge_probe_point), "side: no climb");
            continue;
        }

        int normal_hits = 0;
        for (const IVec2& normal_probe_point : normal_probe_points) {
            const std::optional<WorldTileQueryResult> normal_query =
                QueryTileAtWorldPos(state.stage, normal_probe_point);
            if (IsClimbableTileQuery(state.stage, normal_query)) {
                normal_hits += 1;
            }
        }
        if (normal_hits < kGroundedDownSideAttachRequiredProbeHits) {
            AddClimbDebugLabel(state, ToVec2(edge_probe_point), "side: probe hits low");
            continue;
        }

        const ClimbAnchor edge_anchor{.tile_pos = edge_query->tile_pos};
        if (CanAttachDownToClimbAnchor(edge_anchor, state)) {
            AddClimbDebugLabel(state, ToVec2(edge_probe_point), "side: attach");
            return edge_anchor;
        }
        AddClimbDebugLabel(state, ToVec2(edge_probe_point), "side: below not climb");
    }

    return std::nullopt;
}

bool CanAttachDownToClimbAnchor(const ClimbAnchor& climb_anchor, const State& state) {
    const std::optional<WorldTileQueryResult> below_query = QueryTileAtTilePos(
        state.stage,
        IVec2::New(climb_anchor.tile_pos.x, climb_anchor.tile_pos.y + 1)
    );
    return IsClimbableTileQuery(state.stage, below_query);
}

void SnapEntityToClimbTileCenterline(Entity& entity, const IVec2& tile_pos) {
    Vec2 center = entity.GetCenter();
    center.x = static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8);
    entity.SetCenter(center);
}

bool HasClimbableTileAtPosition(
    const Entity& entity,
    const Vec2& pos,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    return GetClimbAnchorAtPosition(entity, pos, state, tuning).has_value();
}

int GetAllowedClimbUpPixels(
    const Entity& entity,
    const State& state,
    const JumpAndClimbTuning& tuning,
    int max_pixels
) {
    int allowed_pixels = 0;
    for (int step = 1; step <= max_pixels; ++step) {
        const Vec2 next_pos = entity.pos + Vec2::New(0.0F, -static_cast<float>(step));
        if (!HasClimbableTileAtPosition(entity, next_pos, state, tuning)) {
            break;
        }
        allowed_pixels = step;
    }
    return allowed_pixels;
}

void AddClimbDebugAnnotations(const Entity& entity, State& state, const JumpAndClimbTuning& tuning) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(entity, entity.pos, tuning);
    const std::array<std::pair<const char*, Vec2>, 3> probe_points = {{
        {"climb L", probes.left},
        {"climb C", probes.center},
        {"climb R", probes.right},
    }};

    for (const auto& [label, probe_point] : probe_points) {
        const bool is_climbable =
            IsClimbableTileQuery(state.stage, QueryTileAtWorldPos(state.stage, ToIVec2(probe_point)));
        const DebugAnnotationColor color = is_climbable
                                               ? DebugAnnotationColor{64, 255, 128, 255}
                                               : DebugAnnotationColor{255, 64, 64, 255};
        const AABB point_aabb = AABB::New(probe_point, probe_point);
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = point_aabb,
            .color = color,
        });
        state.AddDebugLabelAnnotation(DebugLabelAnnotation{
            .world_pos = probe_point,
            .text = label,
            .color = color,
        });
    }
}

bool IsHangableImpassableInRect(const Vec2& tl, const Vec2& br, const State& state, VID self_vid) {
    const AABB area = AABB::New(tl, br);
    const Vec2 anchor = (tl + br) / 2.0F;
    for (const VID& other_vid : QueryEntitiesInAabb(state, area, self_vid)) {
        const Entity* const other = state.entity_manager.GetEntity(other_vid);
        if (other == nullptr || !other->active || !other->impassable || !other->can_be_hung_on) {
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

    if (check_entities && IsHangableImpassableInRect(tl, br, state, self_vid)) {
        return true;
    }

    return false;
}

bool EntityHasHangGloves(const Entity& entity) {
    if (entity.can_hang_wall) {
        return true;
    }
    if (HasEffect(entity, EffectId::Gloves)) {
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

void StartEntityJump(Entity& entity, const JumpAndClimbTuning& tuning) {
    const float jump_impulse =
        GetModifiedEffectValue(entity, EffectModifierTarget::JumpImpulse, tuning.jump_impulse);
    entity.vel.y = -jump_impulse;
    entity.jump_delay_frame_count = tuning.jump_delay_frames;
    entity.jump_hold_gravity_frames_remaining = tuning.jump_hold_gravity_frames;
    entity.jumped_this_frame = true;
}

void PlayJumpSoundsForEntity(State& state, Entity& entity) {
    (void)PlayEntityCenterSoundEmitter(state, entity, audio_asset_ids::Jump);
    if (HasEffect(entity, EffectId::SpringShoes)) {
        (void)PlayEntityCenterSoundEmitter(
            state,
            entity,
            audio_asset_ids::SpringShoe,
            AudioEmitterPlayParams{.volume_scale = kSpringShoeMovementSoundVolume}
        );
    }
}

void ApplyAirGravity(
    Entity& entity,
    const State& state,
    const controls::ControlIntent& control,
    const JumpAndClimbTuning& tuning
) {
    float gravity = state.stage.gravity * tuning.gravity_scale;
    if (tuning.jump_hold_gravity_frames > 0 && entity.jump_hold_gravity_frames_remaining > 0 &&
        control.jump && entity.vel.y < 0.0F) {
        gravity = 0.0F;
        entity.jump_hold_gravity_frames_remaining -= 1;
    } else {
        entity.jump_hold_gravity_frames_remaining = 0;
    }

    entity.acc.y += gravity;
}

void DetachFromClimb(Entity& entity, const JumpAndClimbTuning& tuning) {
    const bool was_climbing = entity.IsClimbing();
    SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
    if (was_climbing) {
        entity.climb_detach_cooldown = std::max(
            entity.climb_detach_cooldown,
            tuning.climb_detach_cooldown_frames
        );
    }
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

bool MovementFlagsHave(std::uint32_t movement_flags, EntityMovementFlag movement_flag) {
    const std::uint32_t bit = 1U << static_cast<std::uint32_t>(movement_flag);
    return (movement_flags & bit) != 0;
}

bool IsClaimCloseEnough(const Entity& entity, Vec2 claimed_pos) {
    const Vec2 delta = claimed_pos - entity.pos;
    const float distance_sq = delta.x * delta.x + delta.y * delta.y;
    return distance_sq <= kLocomotionClaimMaxDistancePx * kLocomotionClaimMaxDistancePx;
}

bool IsCoordinatorExternallyControllingLocomotion(const Entity& entity) {
    return entity.condition != EntityCondition::Normal ||
        entity.held_by_vid.has_value() ||
        entity.attachment_mode != AttachmentMode::None ||
        entity.thrown_by.has_value() ||
        entity.marked_for_destruction ||
        !entity.active;
}

bool IsClaimVelocityPlausible(const Entity& candidate) {
    return std::abs(candidate.vel.x) <= kLocomotionClaimMaxHorizontalVelocityPx &&
        std::abs(candidate.vel.y) <= kLocomotionClaimMaxVerticalVelocityPx;
}

bool IsCandidateAabbFreeOfSolidTiles(const Entity& candidate, const State& state) {
    const AABB aabb = candidate.GetAABB();
    for (const WorldTileQueryResult& tile_query :
         QueryTilesInWorldRect(state.stage, ToIVec2(aabb.tl), ToIVec2(aabb.br))) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return false;
        }
    }
    return true;
}

bool CandidateHasGroundSupport(Entity candidate, const Stage& stage) {
    candidate.grounded = false;
    candidate.SetGrounded(stage);
    return candidate.grounded;
}

bool IsPlausibleFreeBodyCandidate(
    const Entity& current_entity,
    const Entity& candidate,
    const State& state,
    bool claimed_grounded
) {
    if (IsCoordinatorExternallyControllingLocomotion(current_entity)) {
        return false;
    }
    if (!IsClaimVelocityPlausible(candidate)) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }
    if (claimed_grounded && !CandidateHasGroundSupport(candidate, state.stage)) {
        return false;
    }
    return true;
}

bool IsPlausibleHangCandidate(
    const Entity& current_entity,
    Entity& candidate,
    State& state,
    std::optional<LeftOrRight> claimed_hang_side
) {
    if (IsCoordinatorExternallyControllingLocomotion(current_entity)) {
        return false;
    }
    if (!claimed_hang_side.has_value()) {
        return false;
    }
    if (!candidate.can_hang_ledge && !EntityHasHangGloves(candidate)) {
        return false;
    }
    if (candidate.condition != EntityCondition::Normal) {
        return false;
    }
    if (candidate.grounded || candidate.IsClimbing()) {
        return false;
    }
    if (current_entity.vel.y < kLocomotionClaimUpwardVelocityGrace &&
        candidate.vel.y < kLocomotionClaimUpwardVelocityGrace) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }

    const bool left_side = *claimed_hang_side == LeftOrRight::Left;
    const AABB aabb = candidate.GetAABB();
    const bool top_blocked = IsBlockedForHangProbe(
        Vec2::New(aabb.tl.x, aabb.tl.y - 1.0F),
        Vec2::New(aabb.br.x, aabb.tl.y - 1.0F),
        state,
        true,
        true,
        false,
        candidate.vid
    );
    if (top_blocked || !IsSideBlockedForHang(candidate, state, left_side, true, true)) {
        return false;
    }

    if (CanCornerHangOnSide(candidate, state, left_side, true, true)) {
        return true;
    }

    const bool has_gloves = EntityHasHangGloves(candidate);
    return has_gloves && CanGloveHangBelowCorner(candidate, state, left_side, true, true);
}

bool IsPlausibleClimbCandidate(
    const Entity& current_entity,
    Entity& candidate,
    State& state,
    const JumpAndClimbTuning& tuning
) {
    if (IsCoordinatorExternallyControllingLocomotion(current_entity)) {
        return false;
    }
    if (candidate.condition != EntityCondition::Normal) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }
    return GetClimbAnchor(candidate, state, tuning).has_value();
}

bool IsPlausibleJumpCandidate(
    const Entity& current_entity,
    const Entity& candidate,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    if (IsCoordinatorExternallyControllingLocomotion(current_entity)) {
        return false;
    }
    if (candidate.condition != EntityCondition::Normal) {
        return false;
    }
    if (current_entity.condition != EntityCondition::Normal) {
        return false;
    }
    if (!current_entity.grounded &&
        current_entity.coyote_time == 0 &&
        !current_entity.IsHanging() &&
        !current_entity.IsClimbing()) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }

    const float allowed_impulse = std::max(
        GetModifiedEffectValue(
            current_entity,
            EffectModifierTarget::JumpImpulse,
            tuning.jump_impulse,
            &state
        ) + 2.0F,
        6.0F
    );
    return candidate.vel.y < -0.5F && candidate.vel.y >= -allowed_impulse;
}

bool TryCaptureHdHang(
    std::size_t entity_idx,
    State& state,
    const JumpAndClimbTuning& tuning,
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

    const bool input_try_left = IsTryingToHangOnSide(entity, state, true);
    const bool input_try_right = IsTryingToHangOnSide(entity, state, false);
    const bool try_left = tuning.auto_ledge_grab || input_try_left;
    const bool try_right = tuning.auto_ledge_grab || input_try_right;
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
            if (!input_try_left) {
                return false;
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
            if (!input_try_right) {
                return false;
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

bool TryApplySwimImpulse(Entity& entity, State& state, Audio& audio) {
    const float swim_impulse =
        GetModifiedEffectValue(entity, EffectModifierTarget::SwimImpulse, 0.0F, &state);
    if (swim_impulse <= 0.0F) {
        return false;
    }

    const bool play_sound = entity.vel.y > -swim_impulse * 0.5F;
    entity.vel.y = std::min(entity.vel.y, -swim_impulse);
    entity.grounded = false;
    entity.coyote_time = 0;
    entity.jump_delay_frame_count = 0;
    entity.jump_hold_gravity_frames_remaining = 0;
    entity.jumped_this_frame = true;
    if (play_sound) {
        PlayJumpSoundsForEntity(state, entity);
    }
    (void)audio;
    return true;
}

bool TryApplyPlausibleLocomotionClaim(
    Entity& entity,
    State& state,
    const JumpAndClimbTuning& tuning,
    Vec2 claimed_pos,
    Vec2 claimed_vel,
    Vec2 claimed_acc,
    std::uint32_t claimed_movement_flags,
    bool claimed_grounded,
    std::optional<LeftOrRight> claimed_hang_side,
    std::uint32_t claimed_coyote_time,
    std::uint32_t claimed_fall_timer,
    std::uint32_t claimed_hang_count,
    std::uint32_t claimed_climb_detach_cooldown
) {
    if (!IsClaimCloseEnough(entity, claimed_pos)) {
        return false;
    }

    Entity candidate = entity;
    candidate.pos = claimed_pos;
    candidate.vel = claimed_vel;
    candidate.acc = claimed_acc;
    candidate.grounded = claimed_grounded;
    candidate.movement_flags = claimed_movement_flags;
    candidate.hang_side = claimed_hang_side;
    candidate.coyote_time = claimed_coyote_time;
    candidate.fall_timer = claimed_fall_timer;
    candidate.hang_count = claimed_hang_count;
    candidate.climb_detach_cooldown = claimed_climb_detach_cooldown;

    const bool claimed_hanging =
        MovementFlagsHave(claimed_movement_flags, EntityMovementFlag::Hanging) &&
        claimed_hang_side.has_value();
    const bool claimed_climbing =
        MovementFlagsHave(claimed_movement_flags, EntityMovementFlag::Climbing);
    const bool claimed_jump =
        !claimed_hanging && !claimed_climbing && !candidate.grounded && claimed_vel.y < -0.5F;

    if (claimed_hanging && IsPlausibleHangCandidate(entity, candidate, state, claimed_hang_side)) {
        entity.pos = claimed_pos;
        entity.vel = Vec2::New(claimed_vel.x, 0.0F);
        entity.acc = Vec2::New(0.0F, 0.0F);
        entity.grounded = false;
        entity.hang_side = claimed_hang_side;
        SetMovementFlag(entity, EntityMovementFlag::Hanging, true);
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
        entity.coyote_time = std::max(claimed_coyote_time, kHangCoyoteTimeFrames);
        entity.fall_timer = 0;
        entity.hang_count = claimed_hang_count;
        entity.climb_detach_cooldown = claimed_climb_detach_cooldown;
        if (claimed_hang_side.has_value()) {
            entity.facing = *claimed_hang_side;
        }
        return true;
    }

    if (claimed_climbing && IsPlausibleClimbCandidate(entity, candidate, state, tuning)) {
        entity.pos = claimed_pos;
        entity.vel = claimed_vel;
        entity.acc = claimed_acc;
        entity.grounded = false;
        entity.hang_side.reset();
        SetMovementFlag(entity, EntityMovementFlag::Climbing, true);
        SetMovementFlag(entity, EntityMovementFlag::Hanging, false);
        entity.coyote_time = claimed_coyote_time;
        entity.fall_timer = 0;
        entity.hang_count = claimed_hang_count;
        entity.climb_detach_cooldown = claimed_climb_detach_cooldown;
        return true;
    }

    if (claimed_jump && IsPlausibleJumpCandidate(entity, candidate, state, tuning)) {
        entity.pos = claimed_pos;
        entity.vel = claimed_vel;
        entity.acc = claimed_acc;
        entity.grounded = false;
        entity.hang_side.reset();
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
        SetMovementFlag(entity, EntityMovementFlag::Hanging, false);
        entity.coyote_time = 0;
        entity.fall_timer = 0;
        entity.jump_hold_gravity_frames_remaining = candidate.jump_hold_gravity_frames_remaining;
        entity.jump_delay_frame_count = candidate.jump_delay_frame_count;
        return true;
    }

    if (!claimed_hanging &&
        !claimed_climbing &&
        IsPlausibleFreeBodyCandidate(entity, candidate, state, claimed_grounded)) {
        entity.pos = claimed_pos;
        entity.vel = claimed_vel;
        entity.acc = claimed_acc;
        entity.grounded = claimed_grounded;
        entity.movement_flags = claimed_movement_flags;
        entity.hang_side = claimed_hang_side;
        entity.coyote_time = claimed_coyote_time;
        entity.fall_timer = claimed_fall_timer;
        entity.hang_count = claimed_hang_count;
        entity.climb_detach_cooldown = claimed_climb_detach_cooldown;
        entity.jump_hold_gravity_frames_remaining = candidate.jump_hold_gravity_frames_remaining;
        entity.jump_delay_frame_count = candidate.jump_delay_frame_count;
        return true;
    }

    return false;
}

void HangHandsStep(std::size_t entity_idx, State& state, const JumpAndClimbTuning& tuning) {
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
        const bool still_on_side = IsSideBlockedForHang(mutable_entity, state, true, true, true);
        const bool glove_corner_grab =
            has_gloves && !still_trying && CanCornerHangOnSide(mutable_entity, state, true, true, true);
        if (!still_on_side || (has_gloves && !still_trying && !glove_corner_grab)) {
            mutable_entity.hang_side.reset();
            SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
            mutable_entity.hang_count = tuning.hang_wall_release_cooldown_frames;
        }
    } else if (mutable_entity.hang_side == LeftOrRight::Right) {
        const bool has_gloves = EntityHasHangGloves(mutable_entity);
        const bool still_trying = IsTryingToHangOnSide(mutable_entity, state, false);
        const bool still_on_side = IsSideBlockedForHang(mutable_entity, state, false, true, true);
        const bool glove_corner_grab =
            has_gloves && !still_trying && CanCornerHangOnSide(mutable_entity, state, false, true, true);
        if (!still_on_side || (has_gloves && !still_trying && !glove_corner_grab)) {
            mutable_entity.hang_side.reset();
            SetMovementFlag(mutable_entity, EntityMovementFlag::Hanging, false);
            mutable_entity.hang_count = tuning.hang_wall_release_cooldown_frames;
        }
    }

    if (!mutable_entity.IsHanging()) {
        TryCaptureHdHang(entity_idx, state, tuning, true, true);
    }

    if (mutable_entity.IsHanging()) {
        mutable_entity.projectile_contact_timer = 0;
        mutable_entity.vel.y = 0.0F;
        mutable_entity.acc.y = 0.0F;
        mutable_entity.grounded = false;
        mutable_entity.coyote_time = kHangCoyoteTimeFrames;
    }
}

void JumpingAndClimbingStep(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    const JumpAndClimbTuning& tuning
) {
    (void)audio;
    GroundedCheck(entity_idx, state, audio, true, true, tuning.coyote_time_frames);

    Entity& entity = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    const bool was_grounded = entity.grounded;
    const bool was_climbing = entity.IsClimbing();
    if (entity.climb_detach_cooldown > 0) {
        entity.climb_detach_cooldown -= 1;
    }

    const std::optional<ClimbAnchor> climb_anchor = GetClimbAnchor(entity, state, tuning);
    std::optional<ClimbAnchor> active_climb_anchor = climb_anchor;
    if (control.down && entity.grounded &&
        (!active_climb_anchor.has_value() ||
         !CanAttachDownToClimbAnchor(*active_climb_anchor, state))) {
        active_climb_anchor = GetGroundedDownClimbAnchor(entity, state, tuning);
    }
    const bool can_climb = active_climb_anchor.has_value();
    bool consume_jump_press = false;
    AddClimbDebugAnnotations(entity, state, tuning);

    if (entity.condition != EntityCondition::Normal) {
        SetMovementFlag(entity, EntityMovementFlag::Climbing, false);
    } else {
        const bool wants_to_attach =
            control.up ||
            (control.down && entity.grounded && active_climb_anchor.has_value() &&
             CanAttachDownToClimbAnchor(*active_climb_anchor, state));
        if (!entity.IsClimbing() && can_climb && entity.climb_detach_cooldown == 0 && wants_to_attach) {
            SetMovementFlag(entity, EntityMovementFlag::Climbing, true);
            entity.grounded = false;
            entity.vel = Vec2::New(0.0F, 0.0F);
            entity.acc = Vec2::New(0.0F, 0.0F);
        }

        if (entity.IsClimbing()) {
            if (!can_climb) {
                DetachFromClimb(entity, tuning);
            } else if (was_climbing && control.down && was_grounded) {
                DetachFromClimb(entity, tuning);
                entity.vel.y = 0.0F;
                entity.acc.y = 0.0F;
                entity.grounded = true;
            } else {
                SnapEntityToClimbTileCenterline(entity, active_climb_anchor->tile_pos);
                entity.grounded = false;
                entity.vel.x = 0.0F;
                entity.acc.x = 0.0F;

                if (control.up && !control.down) {
                    const int max_climb_pixels = static_cast<int>(std::ceil(tuning.climb_speed));
                    const int allowed_up_pixels =
                        GetAllowedClimbUpPixels(entity, state, tuning, max_climb_pixels);
                    entity.vel.y = -std::min(tuning.climb_speed, static_cast<float>(allowed_up_pixels));
                } else if (control.down && !control.up) {
                    entity.vel.y = tuning.climb_speed;
                } else {
                    entity.vel.y = 0.0F;
                }

                if (control.jump_pressed) {
                    DetachFromClimb(entity, tuning);
                    entity.grounded = false;
                    entity.vel.x = 0.0F;
                    entity.acc.y = 0.0F;
                    entity.coyote_time = 0;
                    consume_jump_press = control.down;
                    if (consume_jump_press) {
                        entity.vel.y = 0.0F;
                    } else {
                        if (control.left && !control.right) {
                            entity.vel.x = -tuning.climb_depart_horizontal_speed;
                        } else if (control.right && !control.left) {
                            entity.vel.x = tuning.climb_depart_horizontal_speed;
                        }
                        StartEntityJump(entity, tuning);
                        PlayJumpSoundsForEntity(state, entity);
                    }
                }
            }
        }
    }

    if (entity.IsClimbing() && entity.grounded) {
        DetachFromClimb(entity, tuning);
    }

    if (control.jump_pressed && !consume_jump_press) {
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
                    has_gloves ? tuning.glove_hang_drop_cooldown_frames
                               : tuning.hang_drop_cooldown_frames;
            } else if (jumping_away) {
                entity.hang_count = kHangCountMax;
            } else {
                StartEntityJump(entity, tuning);
                PlayJumpSoundsForEntity(state, entity);
                entity.hang_count = kHangCountMax;
            }
        } else if (TryApplySwimImpulse(entity, state, audio)) {
            // Fluid jump handled; do not also apply the grounded/coyote jump.
        } else if ((entity.grounded && (entity.jump_delay_frame_count == 0)) || entity.coyote_time > 0) {
            StartEntityJump(entity, tuning);
            entity.coyote_time = 0;
            entity.grounded = false;
            PlayJumpSoundsForEntity(state, entity);
        }
    }

    if (entity.jump_delay_frame_count > 0) {
        entity.jump_delay_frame_count -= 1;
    }

    if (!entity.IsClimbing() && !entity.grounded && !entity.IsHanging()) {
        ApplyAirGravity(entity, state, control, tuning);
    }
}

} // namespace splonks::entities::common
