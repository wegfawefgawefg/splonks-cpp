#include "ents/common/common.hpp"

#include "controls.hpp"
#include "sim/fxp.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace splonks::ents::common {

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
    const Ent& ent,
    const Vec2& pos,
    const JumpAndClimbTuning& tuning
) {
    const Vec2 size = ent.GetSize();
    const Vec2 center = pos + (size / 2.0F);
    const float probe_y =
        pos.y + std::min(tuning.climb_probe_bias_pixels, std::max(0.0F, size.y - 1.0F));
    const float horizontal_offset = (size.x * 0.5F) * std::max(0.0F, tuning.climb_probe_x_scale);
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
    const Ent& ent,
    const Vec2& pos,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(ent, pos, tuning);
    const std::array<IVec2, 3> probe_points = {
        ToIVec2(probes.left),
        ToIVec2(probes.center),
        ToIVec2(probes.right),
    };

    std::optional<IVec2> best_tile = std::nullopt;
    int best_hits = 0;
    float best_score = 0.0F;
    const Vec2 ent_center = pos + (ent.GetSize() / 2.0F);

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
        const float dx = std::abs(tile_center.x - ent_center.x);
        const float dy = std::abs(tile_center.y - ent_center.y);
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
    const Ent& ent,
    const State& state,
    const JumpAndClimbTuning& tuning,
    const std::array<IVec2, 3>& probe_points
) {
    std::optional<IVec2> best_tile = std::nullopt;
    int best_hits = 0;
    float best_score = 0.0F;
    const Vec2 ent_center = ent.GetCenter();

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
        const float dx = std::abs(tile_center.x - ent_center.x);
        const float dy = std::abs(tile_center.y - ent_center.y);
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
    const Ent& ent,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    return GetClimbAnchorAtPosition(ent, ent.pos, state, tuning);
}

std::optional<ClimbAnchor> GetGroundedDownClimbAnchor(
    const Ent& ent,
    State& state,
    const JumpAndClimbTuning& tuning
) {
    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(ent, ent.pos, tuning);
    const AABB aabb = ent.GetAABB();
    const std::array<IVec2, 3> normal_probe_points = {
        ToIVec2(probes.left),
        ToIVec2(probes.center),
        ToIVec2(probes.right),
    };
    const int probe_y = FloorToInt(aabb.br.y + 1.0F);
    const std::array<IVec2, 3> probe_points = {
        IVec2::New(FloorToInt(probes.left.x), probe_y),
        IVec2::New(FloorToInt(probes.center.x), probe_y),
        IVec2::New(FloorToInt(probes.right.x), probe_y),
    };

    std::optional<ClimbAnchor> climb_anchor =
        GetClimbAnchorFromProbePoints(ent, state, tuning, probe_points);
    if (!climb_anchor.has_value() || !CanAttachDownToClimbAnchor(*climb_anchor, state)) {
        climb_anchor = std::nullopt;
    }
    if (climb_anchor.has_value()) {
        return climb_anchor;
    }

    const std::array<IVec2, 2> edge_probe_points = {
        IVec2::New(FloorToInt(aabb.tl.x - 1.0F), probe_y),
        IVec2::New(FloorToInt(aabb.br.x + 1.0F), probe_y),
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

void SnapEntToClimbTileCenterline(Ent& ent, const IVec2& tile_pos) {
    Vec2 center = ent.GetCenter();
    center.x = static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8);
    ent.SetCenter(center);
}

bool HasClimbableTileAtPosition(
    const Ent& ent,
    const Vec2& pos,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    return GetClimbAnchorAtPosition(ent, pos, state, tuning).has_value();
}

int GetAllowedClimbUpPixels(
    const Ent& ent,
    const State& state,
    const JumpAndClimbTuning& tuning,
    int max_pixels
) {
    int allowed_pixels = 0;
    for (int step = 1; step <= max_pixels; ++step) {
        const Vec2 next_pos = ent.pos + Vec2::New(0.0F, -static_cast<float>(step));
        if (!HasClimbableTileAtPosition(ent, next_pos, state, tuning)) {
            break;
        }
        allowed_pixels = step;
    }
    return allowed_pixels;
}

void AddClimbDebugAnnotations(const Ent& ent, State& state, const JumpAndClimbTuning& tuning) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    const ClimbProbePoints probes = GetClimbProbePointsAtPosition(ent, ent.pos, tuning);
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
    for (const VID& other_vid : QueryEntsInAabb(state, area, self_vid)) {
        const Ent* const other = state.ents.GetEnt(other_vid);
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
    bool check_ents,
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

    if (check_ents && IsHangableImpassableInRect(tl, br, state, self_vid)) {
        return true;
    }

    return false;
}

bool EntHasHangGloves(const Ent& ent) {
    if (ent.can_hang_wall) {
        return true;
    }
    if (HasEffect(ent, EffectId::Gloves)) {
        return true;
    }
    return false;
}

bool IsTryingToHangOnSide(const Ent& ent, const State& state, bool left_side) {
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(ent, state);
    if (left_side) {
        return control.left && !control.right;
    }
    return control.right && !control.left;
}

void StartEntJump(Ent& ent, const JumpAndClimbTuning& tuning) {
    const float jump_impulse =
        GetModifiedEffectValue(ent, EffectModifierTarget::JumpImpulse, tuning.jump_impulse);
    ent.vel.y = -jump_impulse;
    ent.jump_delay_frame_count = tuning.jump_delay_frames;
    ent.jump_hold_gravity_frames_remaining = tuning.jump_hold_gravity_frames;
    ent.jumped_this_frame = true;
}

void PlayJumpSoundsForEnt(State& state, Ent& ent) {
    (void)PlayEntCenterSoundEmitter(state, ent, audio_asset_ids::Jump);
    if (HasEffect(ent, EffectId::SpringShoes)) {
        (void)PlayEntCenterSoundEmitter(
            state,
            ent,
            audio_asset_ids::SpringShoe,
            AudioEmitterPlayParams{.volume_scale = kSpringShoeMovementSoundVolume}
        );
    }
}

void ApplyAirGravity(
    Ent& ent,
    const State& state,
    const controls::ControlIntent& control,
    const JumpAndClimbTuning& tuning
) {
    float gravity = sim::ToRenderScalar(state.stage.gravity) * tuning.gravity_scale;
    if (tuning.jump_hold_gravity_frames > 0 && ent.jump_hold_gravity_frames_remaining > 0 &&
        control.jump && ent.vel.y < 0.0F) {
        gravity = 0.0F;
        ent.jump_hold_gravity_frames_remaining -= 1;
    } else {
        ent.jump_hold_gravity_frames_remaining = 0;
    }

    ent.acc.y += gravity;
}

void DetachFromClimb(Ent& ent, const JumpAndClimbTuning& tuning) {
    const bool was_climbing = ent.IsClimbing();
    SetMovementFlag(ent, EntMovementFlag::Climbing, false);
    if (was_climbing) {
        ent.climb_detach_cooldown = std::max(
            ent.climb_detach_cooldown,
            tuning.climb_detach_cooldown_frames
        );
    }
}

bool IsSideBlockedForHang(
    const Ent& ent,
    const State& state,
    bool left_side,
    bool check_tiles,
    bool check_ents
) {
    const AABB aabb = ent.GetAABB();
    const Vec2 wall_tl = left_side ? Vec2::New(aabb.tl.x - 1.0F, aabb.tl.y)
                                   : Vec2::New(aabb.br.x, aabb.tl.y);
    const Vec2 wall_br = left_side ? Vec2::New(aabb.tl.x, aabb.br.y)
                                   : Vec2::New(aabb.br.x + 1.0F, aabb.br.y);
    return IsBlockedForHangProbe(
        wall_tl,
        wall_br,
        state,
        check_tiles,
        check_ents,
        true,
        ent.vid
    );
}

bool IsHdHangProbeBlocked(
    const Ent& ent,
    State& state,
    float x,
    float y,
    bool check_tiles,
    bool check_ents,
    bool use_hangable_tiles
) {
    return IsBlockedForHangProbe(
        Vec2::New(x, y),
        Vec2::New(x, y),
        state,
        check_tiles,
        check_ents,
        use_hangable_tiles,
        ent.vid
    );
}

bool CanCornerHangOnSide(
    const Ent& ent,
    State& state,
    bool left_side,
    bool check_tiles,
    bool check_ents
) {
    const AABB aabb = ent.GetAABB();
    const float side_x = left_side ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
    const float upper_probe_y_a = aabb.tl.y + 2.0F;
    const float upper_probe_y_b = aabb.tl.y + 3.0F;
    const float center_x = aabb.tl.x + static_cast<float>(FloorToInt(ent.GetSize().x / 2.0F));
    const float below_probe_y = aabb.br.y + 1.0F;

    const bool upper_probe_blocked =
        IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_a, check_tiles, check_ents, true) ||
        IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_b, check_tiles, check_ents, true);
    const bool above_probe_blocked =
        IsHdHangProbeBlocked(ent, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_ents, false);
    const bool below_probe_blocked =
        IsHdHangProbeBlocked(ent, state, center_x, below_probe_y, check_tiles, check_ents, false);

    return upper_probe_blocked && !above_probe_blocked && !below_probe_blocked;
}

bool CanGloveHangBelowCorner(
    const Ent& ent,
    State& state,
    bool left_side,
    bool check_tiles,
    bool check_ents
) {
    const AABB aabb = ent.GetAABB();
    const float side_x = left_side ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
    const int start_y = FloorToInt(aabb.tl.y) - 1;
    const int end_y = FloorToInt(aabb.br.y);

    for (int y = start_y; y <= end_y; ++y) {
        if (IsHdHangProbeBlocked(
                ent,
                state,
                side_x,
                static_cast<float>(y),
                check_tiles,
                check_ents,
                true
            )) {
            return aabb.tl.y >= static_cast<float>(y);
        }
    }

    return false;
}

bool MovementFlagsHave(std::uint32_t movement_flags, EntMovementFlag movement_flag) {
    const std::uint32_t bit = 1U << static_cast<std::uint32_t>(movement_flag);
    return (movement_flags & bit) != 0;
}

bool IsClaimCloseEnough(const Ent& ent, Vec2 claimed_pos) {
    const Vec2 delta = claimed_pos - ent.pos;
    const float distance_sq = delta.x * delta.x + delta.y * delta.y;
    return distance_sq <= kLocomotionClaimMaxDistancePx * kLocomotionClaimMaxDistancePx;
}

bool IsHostExternallyControllingLocomotion(const Ent& ent) {
    return ent.condition != EntCondition::Normal ||
        ent.held_by_vid.has_value() ||
        ent.attach_mode != AttachMode::None ||
        ent.thrown_by.has_value() ||
        ent.marked_for_destruction ||
        !ent.active;
}

bool IsClaimVelocityPlausible(const Ent& candidate) {
    return std::abs(candidate.vel.x) <= kLocomotionClaimMaxHorizontalVelocityPx &&
        std::abs(candidate.vel.y) <= kLocomotionClaimMaxVerticalVelocityPx;
}

bool IsCandidateAabbFreeOfSolidTiles(const Ent& candidate, const State& state) {
    const AABB aabb = candidate.GetAABB();
    for (const WorldTileQueryResult& tile_query :
         QueryTilesInWorldRect(state.stage, ToIVec2(aabb.tl), ToIVec2(aabb.br))) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return false;
        }
    }
    return true;
}

bool CandidateHasGroundSupport(Ent candidate, const Stage& stage) {
    candidate.grounded = false;
    candidate.SetGrounded(stage);
    return candidate.grounded;
}

bool IsPlausibleFreeBodyCandidate(
    const Ent& current_ent,
    const Ent& candidate,
    const State& state,
    bool claimed_grounded
) {
    if (IsHostExternallyControllingLocomotion(current_ent)) {
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
    const Ent& current_ent,
    Ent& candidate,
    State& state,
    std::optional<Side> claimed_hang_side
) {
    if (IsHostExternallyControllingLocomotion(current_ent)) {
        return false;
    }
    if (!claimed_hang_side.has_value()) {
        return false;
    }
    if (!candidate.can_hang_ledge && !EntHasHangGloves(candidate)) {
        return false;
    }
    if (candidate.condition != EntCondition::Normal) {
        return false;
    }
    if (candidate.grounded || candidate.IsClimbing()) {
        return false;
    }
    if (current_ent.vel.y < kLocomotionClaimUpwardVelocityGrace &&
        candidate.vel.y < kLocomotionClaimUpwardVelocityGrace) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }

    const bool left_side = *claimed_hang_side == Side::Left;
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

    const bool has_gloves = EntHasHangGloves(candidate);
    return has_gloves && CanGloveHangBelowCorner(candidate, state, left_side, true, true);
}

bool IsPlausibleClimbCandidate(
    const Ent& current_ent,
    Ent& candidate,
    State& state,
    const JumpAndClimbTuning& tuning
) {
    if (IsHostExternallyControllingLocomotion(current_ent)) {
        return false;
    }
    if (candidate.condition != EntCondition::Normal) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }
    return GetClimbAnchor(candidate, state, tuning).has_value();
}

bool IsPlausibleJumpCandidate(
    const Ent& current_ent,
    const Ent& candidate,
    const State& state,
    const JumpAndClimbTuning& tuning
) {
    if (IsHostExternallyControllingLocomotion(current_ent)) {
        return false;
    }
    if (candidate.condition != EntCondition::Normal) {
        return false;
    }
    if (current_ent.condition != EntCondition::Normal) {
        return false;
    }
    if (!current_ent.grounded &&
        current_ent.coyote_time == 0 &&
        !current_ent.IsHanging() &&
        !current_ent.IsClimbing()) {
        return false;
    }
    if (!IsCandidateAabbFreeOfSolidTiles(candidate, state)) {
        return false;
    }

    const float allowed_impulse = std::max(
        GetModifiedEffectValue(
            current_ent,
            EffectModifierTarget::JumpImpulse,
            tuning.jump_impulse,
            &state
        ) + 2.0F,
        6.0F
    );
    return candidate.vel.y < -0.5F && candidate.vel.y >= -allowed_impulse;
}

bool TryCaptureHdHang(
    std::size_t ent_idx,
    State& state,
    const JumpAndClimbTuning& tuning,
    bool check_tiles,
    bool check_ents
) {
    Ent& ent = state.ents.ents[ent_idx];
    if (!ent.can_hang_ledge && !EntHasHangGloves(ent)) {
        return false;
    }
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(ent, state);
    if (control.no_hang || ent.hang_count > 0) {
        return false;
    }
    if (ent.condition != EntCondition::Normal) {
        return false;
    }
    if (ent.vel.y <= 0.0F) {
        return false;
    }
    if (ent.grounded || ent.IsClimbing() || ent.IsHanging()) {
        return false;
    }

    const bool input_try_left = IsTryingToHangOnSide(ent, state, true);
    const bool input_try_right = IsTryingToHangOnSide(ent, state, false);
    const bool try_left = tuning.auto_ledge_grab || input_try_left;
    const bool try_right = tuning.auto_ledge_grab || input_try_right;
    if (!try_left && !try_right) {
        return false;
    }

    const AABB aabb = ent.GetAABB();
    const bool top_blocked = IsBlockedForHangProbe(
        Vec2::New(aabb.tl.x, aabb.tl.y - 1.0F),
        Vec2::New(aabb.br.x, aabb.tl.y - 1.0F),
        state,
        check_tiles,
        check_ents,
        false,
        ent.vid
    );
    if (top_blocked) {
        return false;
    }

    const bool has_gloves = EntHasHangGloves(ent);
    const float center_x = aabb.tl.x + static_cast<float>(FloorToInt(ent.GetSize().x / 2.0F));
    const float upper_probe_y_a = aabb.tl.y + 2.0F;
    const float upper_probe_y_b = aabb.tl.y + 3.0F;
    const float below_probe_y = aabb.br.y + 1.0F;

    if (try_left && IsSideBlockedForHang(ent, state, true, check_tiles, check_ents)) {
        const float side_x = aabb.tl.x - 1.0F;
        if (has_gloves) {
            if (CanCornerHangOnSide(ent, state, true, check_tiles, check_ents)) {
                ent.pos.y = static_cast<float>(RoundToInt(ent.pos.y / static_cast<float>(kTileSize))) *
                            static_cast<float>(kTileSize);
                ent.hang_side = Side::Left;
                SetMovementFlag(ent, EntMovementFlag::Hanging, true);
                ent.facing = Side::Left;
                ent.vel.y = 0.0F;
                ent.acc.y = 0.0F;
                ent.grounded = false;
                return true;
            }
            if (!input_try_left) {
                return false;
            }
            if (!CanGloveHangBelowCorner(ent, state, true, check_tiles, check_ents)) {
                return false;
            }
            ent.hang_side = Side::Left;
            SetMovementFlag(ent, EntMovementFlag::Hanging, true);
            ent.facing = Side::Left;
            ent.vel.y = 0.0F;
            ent.acc.y = 0.0F;
            ent.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_a, check_tiles, check_ents, true) ||
            IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_b, check_tiles, check_ents, true);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(ent, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_ents, false);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(ent, state, center_x, below_probe_y, check_tiles, check_ents, false);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        ent.pos.y = static_cast<float>(RoundToInt(ent.pos.y / static_cast<float>(kTileSize))) *
                    static_cast<float>(kTileSize);
        ent.hang_side = Side::Left;
        SetMovementFlag(ent, EntMovementFlag::Hanging, true);
        ent.facing = Side::Left;
        ent.vel.y = 0.0F;
        ent.acc.y = 0.0F;
        ent.grounded = false;
        return true;
    }

    if (try_right && IsSideBlockedForHang(ent, state, false, check_tiles, check_ents)) {
        const float side_x = aabb.br.x + 1.0F;
        if (has_gloves) {
            if (CanCornerHangOnSide(ent, state, false, check_tiles, check_ents)) {
                ent.pos.y = static_cast<float>(RoundToInt(ent.pos.y / static_cast<float>(kTileSize))) *
                            static_cast<float>(kTileSize);
                ent.hang_side = Side::Right;
                SetMovementFlag(ent, EntMovementFlag::Hanging, true);
                ent.facing = Side::Right;
                ent.vel.y = 0.0F;
                ent.acc.y = 0.0F;
                ent.grounded = false;
                return true;
            }
            if (!input_try_right) {
                return false;
            }
            if (!CanGloveHangBelowCorner(ent, state, false, check_tiles, check_ents)) {
                return false;
            }
            ent.hang_side = Side::Right;
            SetMovementFlag(ent, EntMovementFlag::Hanging, true);
            ent.facing = Side::Right;
            ent.vel.y = 0.0F;
            ent.acc.y = 0.0F;
            ent.grounded = false;
            return true;
        }
        const bool upper_probe_blocked =
            IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_a, check_tiles, check_ents, true) ||
            IsHdHangProbeBlocked(ent, state, side_x, upper_probe_y_b, check_tiles, check_ents, true);
        const bool above_probe_blocked =
            IsHdHangProbeBlocked(ent, state, side_x, aabb.tl.y - 1.0F, check_tiles, check_ents, false);
        const bool below_probe_blocked =
            IsHdHangProbeBlocked(ent, state, center_x, below_probe_y, check_tiles, check_ents, false);
        if (!upper_probe_blocked) {
            return false;
        }
        if (!has_gloves && (above_probe_blocked || below_probe_blocked)) {
            return false;
        }

        ent.pos.y = static_cast<float>(RoundToInt(ent.pos.y / static_cast<float>(kTileSize))) *
                    static_cast<float>(kTileSize);
        ent.hang_side = Side::Right;
        SetMovementFlag(ent, EntMovementFlag::Hanging, true);
        ent.facing = Side::Right;
        ent.vel.y = 0.0F;
        ent.acc.y = 0.0F;
        ent.grounded = false;
        return true;
    }

    return false;
}

} // namespace

bool TryApplySwimImpulse(Ent& ent, State& state, Audio& audio) {
    const float swim_impulse =
        GetModifiedEffectValue(ent, EffectModifierTarget::SwimImpulse, 0.0F, &state);
    if (swim_impulse <= 0.0F) {
        return false;
    }

    const bool play_sound = ent.vel.y > -swim_impulse * 0.5F;
    ent.vel.y = std::min(ent.vel.y, -swim_impulse);
    ent.grounded = false;
    ent.coyote_time = 0;
    ent.jump_delay_frame_count = 0;
    ent.jump_hold_gravity_frames_remaining = 0;
    ent.jumped_this_frame = true;
    if (play_sound) {
        PlayJumpSoundsForEnt(state, ent);
    }
    (void)audio;
    return true;
}

bool TryApplyPlausibleLocomotionClaim(
    Ent& ent,
    State& state,
    const JumpAndClimbTuning& tuning,
    Vec2 claimed_pos,
    Vec2 claimed_vel,
    Vec2 claimed_acc,
    std::uint32_t claimed_movement_flags,
    bool claimed_grounded,
    std::optional<Side> claimed_hang_side,
    std::uint32_t claimed_coyote_time,
    std::uint32_t claimed_fall_timer,
    std::uint32_t claimed_hang_count,
    std::uint32_t claimed_climb_detach_cooldown
) {
    if (!IsClaimCloseEnough(ent, claimed_pos)) {
        return false;
    }

    Ent candidate = ent;
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
        MovementFlagsHave(claimed_movement_flags, EntMovementFlag::Hanging) &&
        claimed_hang_side.has_value();
    const bool claimed_climbing =
        MovementFlagsHave(claimed_movement_flags, EntMovementFlag::Climbing);
    const bool claimed_jump =
        !claimed_hanging && !claimed_climbing && !candidate.grounded && claimed_vel.y < -0.5F;

    if (claimed_hanging && IsPlausibleHangCandidate(ent, candidate, state, claimed_hang_side)) {
        ent.pos = claimed_pos;
        ent.vel = Vec2::New(claimed_vel.x, 0.0F);
        ent.acc = Vec2::New(0.0F, 0.0F);
        ent.grounded = false;
        ent.hang_side = claimed_hang_side;
        SetMovementFlag(ent, EntMovementFlag::Hanging, true);
        SetMovementFlag(ent, EntMovementFlag::Climbing, false);
        ent.coyote_time = std::max(claimed_coyote_time, kHangCoyoteTimeFrames);
        ent.fall_timer = 0;
        ent.hang_count = claimed_hang_count;
        ent.climb_detach_cooldown = claimed_climb_detach_cooldown;
        if (claimed_hang_side.has_value()) {
            ent.facing = *claimed_hang_side;
        }
        return true;
    }

    if (claimed_climbing && IsPlausibleClimbCandidate(ent, candidate, state, tuning)) {
        ent.pos = claimed_pos;
        ent.vel = claimed_vel;
        ent.acc = claimed_acc;
        ent.grounded = false;
        ent.hang_side.reset();
        SetMovementFlag(ent, EntMovementFlag::Climbing, true);
        SetMovementFlag(ent, EntMovementFlag::Hanging, false);
        ent.coyote_time = claimed_coyote_time;
        ent.fall_timer = 0;
        ent.hang_count = claimed_hang_count;
        ent.climb_detach_cooldown = claimed_climb_detach_cooldown;
        return true;
    }

    if (claimed_jump && IsPlausibleJumpCandidate(ent, candidate, state, tuning)) {
        ent.pos = claimed_pos;
        ent.vel = claimed_vel;
        ent.acc = claimed_acc;
        ent.grounded = false;
        ent.hang_side.reset();
        SetMovementFlag(ent, EntMovementFlag::Climbing, false);
        SetMovementFlag(ent, EntMovementFlag::Hanging, false);
        ent.coyote_time = 0;
        ent.fall_timer = 0;
        ent.jump_hold_gravity_frames_remaining = candidate.jump_hold_gravity_frames_remaining;
        ent.jump_delay_frame_count = candidate.jump_delay_frame_count;
        return true;
    }

    if (!claimed_hanging &&
        !claimed_climbing &&
        IsPlausibleFreeBodyCandidate(ent, candidate, state, claimed_grounded)) {
        ent.pos = claimed_pos;
        ent.vel = claimed_vel;
        ent.acc = claimed_acc;
        ent.grounded = claimed_grounded;
        ent.movement_flags = claimed_movement_flags;
        ent.hang_side = claimed_hang_side;
        ent.coyote_time = claimed_coyote_time;
        ent.fall_timer = claimed_fall_timer;
        ent.hang_count = claimed_hang_count;
        ent.climb_detach_cooldown = claimed_climb_detach_cooldown;
        ent.jump_hold_gravity_frames_remaining = candidate.jump_hold_gravity_frames_remaining;
        ent.jump_delay_frame_count = candidate.jump_delay_frame_count;
        return true;
    }

    return false;
}

void HangHandsStep(std::size_t ent_idx, State& state, const JumpAndClimbTuning& tuning) {
    Ent& mutable_ent = state.ents.ents[ent_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(mutable_ent, state);
    if (control.no_hang) {
        mutable_ent.hang_side.reset();
        SetMovementFlag(mutable_ent, EntMovementFlag::Hanging, false);
    }
    if (mutable_ent.condition != EntCondition::Normal) {
        mutable_ent.hang_side.reset();
        SetMovementFlag(mutable_ent, EntMovementFlag::Hanging, false);
    }

    if (mutable_ent.hang_side == Side::Left) {
        const bool has_gloves = EntHasHangGloves(mutable_ent);
        const bool still_trying = IsTryingToHangOnSide(mutable_ent, state, true);
        const bool still_on_side = IsSideBlockedForHang(mutable_ent, state, true, true, true);
        const bool glove_corner_grab =
            has_gloves && !still_trying && CanCornerHangOnSide(mutable_ent, state, true, true, true);
        if (!still_on_side || (has_gloves && !still_trying && !glove_corner_grab)) {
            mutable_ent.hang_side.reset();
            SetMovementFlag(mutable_ent, EntMovementFlag::Hanging, false);
            mutable_ent.hang_count = tuning.hang_wall_release_cooldown_frames;
        }
    } else if (mutable_ent.hang_side == Side::Right) {
        const bool has_gloves = EntHasHangGloves(mutable_ent);
        const bool still_trying = IsTryingToHangOnSide(mutable_ent, state, false);
        const bool still_on_side = IsSideBlockedForHang(mutable_ent, state, false, true, true);
        const bool glove_corner_grab =
            has_gloves && !still_trying && CanCornerHangOnSide(mutable_ent, state, false, true, true);
        if (!still_on_side || (has_gloves && !still_trying && !glove_corner_grab)) {
            mutable_ent.hang_side.reset();
            SetMovementFlag(mutable_ent, EntMovementFlag::Hanging, false);
            mutable_ent.hang_count = tuning.hang_wall_release_cooldown_frames;
        }
    }

    if (!mutable_ent.IsHanging()) {
        TryCaptureHdHang(ent_idx, state, tuning, true, true);
    }

    if (mutable_ent.IsHanging()) {
        mutable_ent.proj_contact_timer = 0;
        mutable_ent.vel.y = 0.0F;
        mutable_ent.acc.y = 0.0F;
        mutable_ent.grounded = false;
        mutable_ent.coyote_time = kHangCoyoteTimeFrames;
    }
}

void JumpingAndClimbingStep(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    const JumpAndClimbTuning& tuning
) {
    (void)audio;
    GroundedCheck(ent_idx, state, audio, true, true, tuning.coyote_time_frames);

    Ent& ent = state.ents.ents[ent_idx];
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(ent, state);
    const bool was_grounded = ent.grounded;
    const bool was_climbing = ent.IsClimbing();
    if (ent.climb_detach_cooldown > 0) {
        ent.climb_detach_cooldown -= 1;
    }

    const std::optional<ClimbAnchor> climb_anchor = GetClimbAnchor(ent, state, tuning);
    std::optional<ClimbAnchor> active_climb_anchor = climb_anchor;
    if (control.down && ent.grounded &&
        (!active_climb_anchor.has_value() ||
         !CanAttachDownToClimbAnchor(*active_climb_anchor, state))) {
        active_climb_anchor = GetGroundedDownClimbAnchor(ent, state, tuning);
    }
    const bool can_climb = active_climb_anchor.has_value();
    bool consume_jump_press = false;
    AddClimbDebugAnnotations(ent, state, tuning);

    if (ent.condition != EntCondition::Normal) {
        SetMovementFlag(ent, EntMovementFlag::Climbing, false);
    } else {
        const bool wants_to_attach =
            control.up ||
            (control.down && ent.grounded && active_climb_anchor.has_value() &&
             CanAttachDownToClimbAnchor(*active_climb_anchor, state));
        if (!ent.IsClimbing() && can_climb && ent.climb_detach_cooldown == 0 && wants_to_attach) {
            SetMovementFlag(ent, EntMovementFlag::Climbing, true);
            ent.grounded = false;
            ent.vel = Vec2::New(0.0F, 0.0F);
            ent.acc = Vec2::New(0.0F, 0.0F);
        }

        if (ent.IsClimbing()) {
            if (!can_climb) {
                DetachFromClimb(ent, tuning);
            } else if (was_climbing && control.down && was_grounded) {
                DetachFromClimb(ent, tuning);
                ent.vel.y = 0.0F;
                ent.acc.y = 0.0F;
                ent.grounded = true;
            } else {
                SnapEntToClimbTileCenterline(ent, active_climb_anchor->tile_pos);
                ent.grounded = false;
                ent.vel.x = 0.0F;
                ent.acc.x = 0.0F;

                if (control.up && !control.down) {
                    const int max_climb_pixels = CeilToInt(tuning.climb_speed);
                    const int allowed_up_pixels =
                        GetAllowedClimbUpPixels(ent, state, tuning, max_climb_pixels);
                    ent.vel.y = -std::min(tuning.climb_speed, static_cast<float>(allowed_up_pixels));
                } else if (control.down && !control.up) {
                    ent.vel.y = tuning.climb_speed;
                } else {
                    ent.vel.y = 0.0F;
                }

                if (control.jump_pressed) {
                    DetachFromClimb(ent, tuning);
                    ent.grounded = false;
                    ent.vel.x = 0.0F;
                    ent.acc.y = 0.0F;
                    ent.coyote_time = 0;
                    consume_jump_press = control.down;
                    if (consume_jump_press) {
                        ent.vel.y = 0.0F;
                    } else {
                        if (control.left && !control.right) {
                            ent.vel.x = -tuning.climb_depart_horizontal_speed;
                        } else if (control.right && !control.left) {
                            ent.vel.x = tuning.climb_depart_horizontal_speed;
                        }
                        StartEntJump(ent, tuning);
                        PlayJumpSoundsForEnt(state, ent);
                    }
                }
            }
        }
    }

    if (ent.IsClimbing() && ent.grounded) {
        DetachFromClimb(ent, tuning);
    }

    if (control.jump_pressed && !consume_jump_press) {
        if (ent.IsHanging()) {
            const bool jumping_away =
                (ent.hang_side == Side::Right && control.left) ||
                (ent.hang_side == Side::Left && control.right);
            const bool has_gloves = EntHasHangGloves(ent);
            ent.hang_side.reset();
            SetMovementFlag(ent, EntMovementFlag::Hanging, false);
            ent.grounded = false;
            if (control.down) {
                ent.hang_count =
                    has_gloves ? tuning.glove_hang_drop_cooldown_frames
                               : tuning.hang_drop_cooldown_frames;
            } else if (jumping_away) {
                ent.hang_count = kHangCountMax;
            } else {
                StartEntJump(ent, tuning);
                PlayJumpSoundsForEnt(state, ent);
                ent.hang_count = kHangCountMax;
            }
        } else if (TryApplySwimImpulse(ent, state, audio)) {
            // Fluid jump handled; do not also apply the grounded/coyote jump.
        } else if ((ent.grounded && (ent.jump_delay_frame_count == 0)) || ent.coyote_time > 0) {
            StartEntJump(ent, tuning);
            ent.coyote_time = 0;
            ent.grounded = false;
            PlayJumpSoundsForEnt(state, ent);
        }
    }

    if (ent.jump_delay_frame_count > 0) {
        ent.jump_delay_frame_count -= 1;
    }

    if (!ent.IsClimbing() && !ent.grounded && !ent.IsHanging()) {
        ApplyAirGravity(ent, state, control, tuning);
    }
}

} // namespace splonks::ents::common
