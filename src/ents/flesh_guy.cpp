#include "ents/flesh_guy.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "ents/common/common.hpp"
#include "ents/player.hpp"
#include "aframe_id.hpp"
#include "particles/particle_specs.hpp"
#include "particles/scripted_particle.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace splonks::ents::flesh_guy {

namespace {

constexpr float kGroundTargetSpeed = 3.45F;
constexpr float kAirTargetSpeed = 3.65F;
constexpr float kGroundMoveAcc = 0.42F;
constexpr float kAirMoveAcc = 0.24F;
constexpr float kGroundNoInputDecel = 0.35F;
constexpr float kAirNoInputDamping = 0.96F;
constexpr float kJumpImpulse = player::kJumpImpulse * 0.60F;
constexpr float kWallJumpHorizontalSpeed = 3.2F;
constexpr float kGravityScale = 0.50F;
constexpr float kWallSlideGravityScale = 0.28F;
constexpr float kWallSlideMaxFallSpeed = 1.35F;
constexpr float kMaxHorizontalSpeed = 4.25F;
constexpr float kMaxRiseSpeed = 5.0F;
constexpr float kMaxFallSpeed = 6.0F;
constexpr float kWalkAnimVelocityEpsilon = 0.08F;
constexpr float kMeatTileTopperHeight = 7.0F;
constexpr float kMeatTileTopperYOffset = -1.0F;
constexpr float kSideMeatTileInset = (kMeatTileTopperHeight * 0.5F) - 1.0F;
constexpr float kClimbSpeed = 2.1F;
constexpr int kClimbSnapSpeedPixels = 1;

enum class MeatSlimeSurfaceKind {
    Tile,
    LeftBorder,
    RightBorder,
    TopBorder,
    BottomBorder,
};

struct MeatSlimeSurface {
    MeatSlimeSurfaceKind kind = MeatSlimeSurfaceKind::Tile;
    IVec2 key = IVec2::New(0, 0);
    IVec2 tile_pos = IVec2::New(0, 0);
};

int FloorDiv(int value, int divisor) {
    const int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        return quotient - 1;
    }
    return quotient;
}

int FloorToTileCoord(float value) {
    return FloorToInt(value / static_cast<float>(kTileSize));
}

IVec2 WorldPosToUnwrappedTileCoord(const FVec2& world_pos) {
    return IVec2::New(FloorToTileCoord(world_pos.x), FloorToTileCoord(world_pos.y));
}

IVec2 WorldPosToUnwrappedTileCoord(sim::FxVec2 world_pos) {
    return IVec2::New(
        FloorDiv(world_pos.x.to_pixels_floor(), static_cast<int>(kTileSize)),
        FloorDiv(world_pos.y.to_pixels_floor(), static_cast<int>(kTileSize))
    );
}

std::optional<MeatSlimeSurface> QueryCollidableTileOrBorderSurface(
    const Stage& stage,
    const IVec2& unwrapped_tile_pos
) {
    const IVec2 wrapped_tile_pos = stage.WrapTileCoord(unwrapped_tile_pos);
    if (stage.IsTileCoordInside(wrapped_tile_pos.x, wrapped_tile_pos.y)) {
        const Tile& tile = stage.GetTile(
            static_cast<unsigned int>(wrapped_tile_pos.x),
            static_cast<unsigned int>(wrapped_tile_pos.y)
        );
        if (!IsTileCollidable(tile)) {
            return std::nullopt;
        }
        return MeatSlimeSurface{
            .kind = MeatSlimeSurfaceKind::Tile,
            .key = wrapped_tile_pos,
            .tile_pos = wrapped_tile_pos,
        };
    }

    const std::optional<StageBorderSideKind> border_side =
        stage.GetOutOfBoundsSideForTileCoord(unwrapped_tile_pos.x, unwrapped_tile_pos.y);
    if (!border_side.has_value() || !stage.IsBorderSideBlocking(*border_side)) {
        return std::nullopt;
    }

    switch (*border_side) {
    case StageBorderSideKind::Left:
        return MeatSlimeSurface{
            .kind = MeatSlimeSurfaceKind::LeftBorder,
            .key = IVec2::New(-1, wrapped_tile_pos.y),
            .tile_pos = IVec2::New(-1, wrapped_tile_pos.y),
        };
    case StageBorderSideKind::Right:
        return MeatSlimeSurface{
            .kind = MeatSlimeSurfaceKind::RightBorder,
            .key = IVec2::New(static_cast<int>(stage.GetTileWidth()), wrapped_tile_pos.y),
            .tile_pos = IVec2::New(static_cast<int>(stage.GetTileWidth()), wrapped_tile_pos.y),
        };
    case StageBorderSideKind::Top:
        return MeatSlimeSurface{
            .kind = MeatSlimeSurfaceKind::TopBorder,
            .key = IVec2::New(wrapped_tile_pos.x, -1),
            .tile_pos = IVec2::New(wrapped_tile_pos.x, -1),
        };
    case StageBorderSideKind::Bottom:
        return MeatSlimeSurface{
            .kind = MeatSlimeSurfaceKind::BottomBorder,
            .key = IVec2::New(wrapped_tile_pos.x, static_cast<int>(stage.GetTileHeight())),
            .tile_pos = IVec2::New(wrapped_tile_pos.x, static_cast<int>(stage.GetTileHeight())),
        };
    }

    return std::nullopt;
}

std::optional<MeatSlimeSurface> QueryCollidableTileOrBorderSurfaceAtWorldPos(
    const Stage& stage,
    const FVec2& world_pos
) {
    return QueryCollidableTileOrBorderSurface(stage, WorldPosToUnwrappedTileCoord(world_pos));
}

std::optional<MeatSlimeSurface> QueryCollidableTileOrBorderSurfaceAtWorldPos(
    const Stage& stage,
    sim::FxVec2 world_pos
) {
    return QueryCollidableTileOrBorderSurface(stage, WorldPosToUnwrappedTileCoord(world_pos));
}

bool IsClimbableTileQuery(const std::optional<WorldTileQueryResult>& tile_query) {
    return tile_query.has_value() && tile_query->tile != nullptr &&
           GetTileSpec(*tile_query->tile).climbable;
}

std::optional<IVec2> GetClimbTile(const Ent& ent, const State& state) {
    const sim::FxVec2 center = ent.GetSimCenter();
    constexpr sim::Scalar kMaxHorizontalOffset =
        sim::Scalar::from_raw((sim::Scalar::scale * 5) / 2);
    const sim::Scalar horizontal_offset = std::min(
        kMaxHorizontalOffset,
        std::max(sim::Scalar::zero(), (ent.size.x / sim::Scalar::from_int(2)) - sim::Scalar::from_int(1))
    );
    const std::array<sim::FxVec2, 3> probes = {{
        sim::FxVec2{center.x - horizontal_offset, center.y},
        center,
        sim::FxVec2{center.x + horizontal_offset, center.y},
    }};

    for (const sim::FxVec2& probe : probes) {
        const std::optional<WorldTileQueryResult> tile_query =
            QueryTileAtWorldPos(state.stage, sim::ToPixelIVec2Round(probe));
        if (IsClimbableTileQuery(tile_query)) {
            return tile_query->tile_pos;
        }
    }
    return std::nullopt;
}

void SnapToClimbTile(Ent& ent, const IVec2& tile_pos) {
    sim::FxVec2 center = ent.GetSimCenter();
    const sim::Scalar target_x =
        sim::Scalar::from_int(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2));
    const sim::Scalar delta = std::clamp(
        target_x - center.x,
        sim::Scalar::from_int(-kClimbSnapSpeedPixels),
        sim::Scalar::from_int(kClimbSnapSpeedPixels)
    );
    center.x += delta;
    ent.SetSimCenter(center);
}

std::optional<Side> GetWallSlideSide(
    const Ent& ent,
    const controls::ControlIntent& control,
    const State& state,
    const Graphics& graphics
) {
    if (ent.grounded || ent.condition != EntCondition::Normal) {
        return std::nullopt;
    }

    const sim::AABB aabb = ent.GetSimAABB();
    const auto side_blocked = [&](Side side) {
        const bool left = side == Side::Left;
        const sim::Scalar probe_x = left
            ? aabb.tl.x - sim::Scalar::from_int(1)
            : aabb.br.x + sim::Scalar::from_int(1);
        const sim::AABB probe = sim::AABB::from_corners(
            sim::FxVec2{probe_x, aabb.tl.y + sim::Scalar::from_int(1)},
            sim::FxVec2{probe_x, aabb.br.y - sim::Scalar::from_int(1)}
        );
        return AabbHitsBlockingWorldGeometryOrImpassableEnts(state, graphics, probe, ent.vid);
    };

    if (control.left && !control.right && side_blocked(Side::Left)) {
        return Side::Left;
    }
    if (control.right && !control.left && side_blocked(Side::Right)) {
        return Side::Right;
    }
    return std::nullopt;
}

std::optional<MeatSlimeSurface> GetGroundSurface(const Ent& ent, const State& state) {
    if (!ent.grounded) {
        return std::nullopt;
    }

    const sim::AABB aabb = ent.GetSimAABB();
    const sim::FxVec2 center_support_world{
        ent.GetSimCenter().x,
        aabb.br.y + sim::Scalar::from_int(1),
    };
    if (const std::optional<MeatSlimeSurface> center_surface =
            QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_support_world)) {
        return center_surface;
    }

    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, ent.GetSimFeet())) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return MeatSlimeSurface{
                .kind = MeatSlimeSurfaceKind::Tile,
                .key = tile_query.tile_pos,
                .tile_pos = tile_query.tile_pos,
            };
        }
    }
    return std::nullopt;
}

void SpawnMeatSlime(State& state, const FVec2& particle_center, float rotation = 0.0F) {
    ScriptedParticle particle = MakeScriptedParticle(scripted_particle_spec_ids::MeatTileTopper, particle_center);
    if (!particle.active) {
        return;
    }
    particle.rot = rotation;
    state.particles.Add(std::move(particle));
}

FVec2 GetTopMeatSlimeCenter(const IVec2& tile_pos) {
    return FVec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize)) +
            (kMeatTileTopperHeight * 0.5F) + kMeatTileTopperYOffset
    );
}

FVec2 GetTopMeatSlimeCenter(const MeatSlimeSurface& surface, const Stage& stage) {
    (void)stage;
    return GetTopMeatSlimeCenter(surface.tile_pos);
}

FVec2 GetBottomMeatSlimeCenter(const IVec2& tile_pos) {
    return FVec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2)),
        static_cast<float>((tile_pos.y + 1) * static_cast<int>(kTileSize)) -
            (kMeatTileTopperHeight * 0.5F) - kMeatTileTopperYOffset
    );
}

FVec2 GetBottomMeatSlimeCenter(const MeatSlimeSurface& surface, const Stage& stage) {
    (void)stage;
    return GetBottomMeatSlimeCenter(surface.tile_pos);
}

FVec2 GetSideMeatSlimeCenter(const IVec2& tile_pos, Side tile_side) {
    const float tile_left = static_cast<float>(tile_pos.x * static_cast<int>(kTileSize));
    const float tile_top = static_cast<float>(tile_pos.y * static_cast<int>(kTileSize));
    const float x = tile_side == Side::Left
                        ? tile_left + kSideMeatTileInset
                        : tile_left + static_cast<float>(kTileSize) - kSideMeatTileInset;
    return FVec2::New(x, tile_top + static_cast<float>(kTileSize / 2));
}

FVec2 GetSideMeatSlimeCenter(const MeatSlimeSurface& surface, Side tile_side, const Stage& stage) {
    (void)stage;
    return GetSideMeatSlimeCenter(surface.tile_pos, tile_side);
}

void MaybeSpawnTopMeatSlime(Ent& ent, State& state) {
    const std::optional<MeatSlimeSurface> ground_surface = GetGroundSurface(ent, state);
    if (!ground_surface.has_value()) {
        ent.point_label_a = PointLabel::None;
        return;
    }

    if (ent.point_label_a == PointLabel::Target && ent.point_a == ground_surface->key) {
        return;
    }

    ent.point_a = ground_surface->key;
    ent.point_label_a = PointLabel::Target;
    SpawnMeatSlime(state, GetTopMeatSlimeCenter(*ground_surface, state.stage));
}

std::optional<MeatSlimeSurface> GetCeilingSurface(const Ent& ent, const State& state) {
    const sim::AABB aabb = ent.GetSimAABB();
    const sim::Scalar probe_y = aabb.tl.y - sim::Scalar::from_int(1);
    const sim::AABB probe = sim::AABB::from_corners(
        sim::FxVec2{aabb.tl.x + sim::Scalar::from_int(1), probe_y},
        sim::FxVec2{aabb.br.x - sim::Scalar::from_int(1), probe_y}
    );
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, probe)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return MeatSlimeSurface{
                .kind = MeatSlimeSurfaceKind::Tile,
                .key = tile_query.tile_pos,
                .tile_pos = tile_query.tile_pos,
            };
        }
    }

    const sim::FxVec2 center_probe{ent.GetSimCenter().x, probe_y};
    return QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_probe);
}

void MaybeSpawnBottomMeatSlime(Ent& ent, State& state) {
    const std::optional<MeatSlimeSurface> ceiling_surface = GetCeilingSurface(ent, state);
    if (!ceiling_surface.has_value()) {
        ent.point_label_d = PointLabel::None;
        return;
    }

    if (ent.point_label_d == PointLabel::Target && ent.point_d == ceiling_surface->key) {
        return;
    }

    ent.point_d = ceiling_surface->key;
    ent.point_label_d = PointLabel::Target;
    SpawnMeatSlime(state, GetBottomMeatSlimeCenter(*ceiling_surface, state.stage), 180.0F);
}

std::optional<MeatSlimeSurface> GetSideSurface(const Ent& ent, const State& state, Side side) {
    const sim::AABB aabb = ent.GetSimAABB();
    const sim::Scalar probe_x = side == Side::Left
        ? aabb.tl.x - sim::Scalar::from_int(1)
        : aabb.br.x + sim::Scalar::from_int(1);
    const sim::AABB probe = sim::AABB::from_corners(
        sim::FxVec2{probe_x, aabb.tl.y + sim::Scalar::from_int(1)},
        sim::FxVec2{probe_x, aabb.br.y - sim::Scalar::from_int(1)}
    );
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, probe)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return MeatSlimeSurface{
                .kind = MeatSlimeSurfaceKind::Tile,
                .key = tile_query.tile_pos,
                .tile_pos = tile_query.tile_pos,
            };
        }
    }

    const sim::FxVec2 center_probe{probe_x, ent.GetSimCenter().y};
    return QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_probe);
}

void MaybeSpawnSideMeatSlime(Ent& ent, State& state, Side side) {
    const std::optional<MeatSlimeSurface> side_surface = GetSideSurface(ent, state, side);
    PointLabel& label = side == Side::Left ? ent.point_label_b : ent.point_label_c;
    IVec2& cached_tile = side == Side::Left ? ent.point_b : ent.point_c;
    if (!side_surface.has_value()) {
        label = PointLabel::None;
        return;
    }

    if (label == PointLabel::Target && cached_tile == side_surface->key) {
        return;
    }

    cached_tile = side_surface->key;
    label = PointLabel::Target;
    const Side tile_side = side == Side::Left ? Side::Right : Side::Left;
    const float rotation = side == Side::Left ? 90.0F : -90.0F;
    SpawnMeatSlime(state, GetSideMeatSlimeCenter(*side_surface, tile_side, state.stage), rotation);
}

void MaybeSpawnTouchedMeatSlime(Ent& ent, State& state) {
    MaybeSpawnTopMeatSlime(ent, state);
    MaybeSpawnBottomMeatSlime(ent, state);
    MaybeSpawnSideMeatSlime(ent, state, Side::Left);
    MaybeSpawnSideMeatSlime(ent, state, Side::Right);
}

void SetFleshGuyAnim(Ent& ent, std::optional<Side> wall_slide_side = std::nullopt) {
    if (ent.condition == EntCondition::Dead) {
        return;
    }

    if (wall_slide_side.has_value()) {
        if (ent.aframe_animator.anim_id != aframe_ids::FleshGuyWalk) {
            SetAnim(ent, aframe_ids::FleshGuyWalk);
            ent.aframe_animator.SetForcedFrame(0);
        }
        ent.facing = *wall_slide_side;
        ent.aframe_animator.animate = false;
        ent.aframe_animator.loop = false;
        return;
    }

    if (ent.grounded && ent.vel.x.abs() > sim::ToSimScalar(kWalkAnimVelocityEpsilon)) {
        if (ent.aframe_animator.anim_id != aframe_ids::FleshGuyWalk) {
            ent.aframe_animator.PlayLoop(aframe_ids::FleshGuyWalk);
        }
        ent.aframe_animator.animate = true;
        ent.aframe_animator.loop = true;
        return;
    }

    if (ent.aframe_animator.anim_id != aframe_ids::FleshGuy) {
        SetAnim(ent, aframe_ids::FleshGuy);
        ent.aframe_animator.SetForcedFrame(0);
    }
    ent.aframe_animator.animate = false;
    ent.aframe_animator.loop = false;
}

void UpdateWallSlideState(
    Ent& ent,
    const std::optional<Side>& wall_slide_side
) {
    if (wall_slide_side.has_value()) {
        ent.hang_side = wall_slide_side;
    } else if (ent.grounded || ent.coyote_time == 0) {
        ent.hang_side.reset();
    }
    SetMovementFlag(ent, EntMovementFlag::Hanging, wall_slide_side.has_value());
}

void RefreshWallSlideCoyote(Ent& ent, Side side) {
    ent.coyote_time = std::max(ent.coyote_time, player::kCoyoteTimeFrames);
    ent.hang_side = side;
}

std::optional<Side> GetWallSlideCoyoteSide(const Ent& ent) {
    if (ent.coyote_time == 0 || !ent.hang_side.has_value()) {
        return std::nullopt;
    }
    return ent.hang_side;
}

void ClearWallSlideCoyote(Ent& ent) {
    ent.coyote_time = 0;
    ent.hang_side.reset();
    SetMovementFlag(ent, EntMovementFlag::Hanging, false);
}

void StepTravelSoundFleshGuy(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];
    ent.travel_sound_countdown -= ent.dist_traveled_this_frame;

    if ((!ent.grounded && !ent.IsClimbing()) ||
        ent.dist_traveled_this_frame <= sim::Scalar::zero()) {
        return;
    }

    if (ent.travel_sound_countdown < sim::Scalar::zero()) {
        ent.travel_sound_countdown =
            sim::Scalar::from_int(static_cast<std::int32_t>(kWalkerClimberTravelSoundDistInterval));
        const AudioAssetId sound = ent.travel_sound == TravelSound::One
                                       ? audio_asset_ids::FleshGuyStep0
                                       : audio_asset_ids::FleshGuyStep1;
        (void)PlayEntSoundEmitter(state, ent, sound);
        ent.IncTravelSound();
    }
}

} // namespace

extern const EntSpec kFleshGuySpec{
    .type_ = EntType::FleshGuy,
    .size = EntSpecSize(8.0F, 9.0F),
    .health = 400,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_collect_pickups = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = true,
    .can_hang_ledge = false,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .throw_velocity_scale = sim::ToSimScalar(0.5F),
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Right,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .collide_sound = audio_asset_ids::FleshGuyImpact1,
    .death_sound = audio_asset_ids::FleshGuyImpact0,
    .on_death = OnDeathAsFleshGuy,
    .control_logic = ControlEntAsFleshGuy,
    .step_logic = StepEntLogicAsFleshGuy,
    .step_physics = StepEntPhysicsAsFleshGuy,
    .alignment = Alignment::Ally,
    .aframe_animator = AFrameAnimator::New(aframe_ids::FleshGuy),
};

void OnDeathAsFleshGuy(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& flesh_guy = state.ents.ents[ent_idx];
    if (const std::optional<MeatSlimeSurface> ground_surface = GetGroundSurface(flesh_guy, state)) {
        SpawnMeatSlime(state, GetTopMeatSlimeCenter(*ground_surface, state.stage));
    } else if (const std::optional<MeatSlimeSurface> center_surface =
                   QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, flesh_guy.GetSimCenter())) {
        SpawnMeatSlime(state, GetTopMeatSlimeCenter(*center_surface, state.stage));
    }
    (void)world_ops::DeactivateEnt(state, flesh_guy.vid);
}

void ControlEntAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& flesh_guy = state.ents.ents[ent_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEnt(flesh_guy, state);
    if (flesh_guy.condition != EntCondition::Normal) {
        return;
    }

    const bool climbing = flesh_guy.IsClimbing();
    if (climbing) {
        flesh_guy.acc.x = sim::Scalar::zero();
        flesh_guy.vel.x = sim::Scalar::zero();
    } else if (control.left && !control.right) {
        common::AccelerateHorizontallyTowardSpeed(
            flesh_guy,
            state,
            flesh_guy.grounded ? -kGroundTargetSpeed : -kAirTargetSpeed,
            flesh_guy.grounded ? kGroundMoveAcc : kAirMoveAcc
        );
        flesh_guy.facing = Side::Left;
    } else if (control.right && !control.left) {
        common::AccelerateHorizontallyTowardSpeed(
            flesh_guy,
            state,
            flesh_guy.grounded ? kGroundTargetSpeed : kAirTargetSpeed,
            flesh_guy.grounded ? kGroundMoveAcc : kAirMoveAcc
        );
        flesh_guy.facing = Side::Right;
    } else if (flesh_guy.grounded) {
        common::DecelerateHorizontallyToStop(flesh_guy, kGroundNoInputDecel);
    } else {
        flesh_guy.vel.x *= sim::ToSimScalar(kAirNoInputDamping);
    }

    if (control.stop) {
        flesh_guy.acc = sim::FxVec2::zero();
        flesh_guy.vel = sim::FxVec2::zero();
    }
}

void StepEntLogicAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& flesh_guy = state.ents.ents[ent_idx];
    if (flesh_guy.condition == EntCondition::Dead) {
        return;
    }

    StepTravelSoundFleshGuy(ent_idx, state);
    common::CleanupInactiveCarryReferences(ent_idx, state);

    const bool loss_of_control = flesh_guy.condition == EntCondition::Stunned;
    const controls::ControlIntent control = controls::GetControlIntentForEnt(flesh_guy, state);
    const bool walking =
        !loss_of_control &&
        flesh_guy.grounded &&
        (control.left != control.right) &&
        flesh_guy.vel.x.abs() > sim::ToSimScalar(kWalkAnimVelocityEpsilon);
    SetMovementFlag(flesh_guy, EntMovementFlag::Walking, walking);
    SetMovementFlag(flesh_guy, EntMovementFlag::Running, false);

    SetFleshGuyAnim(flesh_guy, flesh_guy.grounded ? std::nullopt : flesh_guy.hang_side);
    common::UpdateCarryAndBackItems(ent_idx, state, graphics, audio);

    if (!loss_of_control) {
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 0, control.bomb_pressed);
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 1, control.rope_pressed);
        common::TryPushBlocks(ent_idx, state, graphics);
    }
}

void StepEntPhysicsAsFleshGuy(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    common::GroundedCheck(ent_idx, state, audio, true, true);

    Ent& flesh_guy = state.ents.ents[ent_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEnt(flesh_guy, state);
    const std::optional<IVec2> climb_tile = GetClimbTile(flesh_guy, state);
    if (flesh_guy.condition != EntCondition::Normal || !climb_tile.has_value()) {
        SetMovementFlag(flesh_guy, EntMovementFlag::Climbing, false);
    } else if (!flesh_guy.IsClimbing() && (control.up || control.down)) {
        SetMovementFlag(flesh_guy, EntMovementFlag::Climbing, true);
        flesh_guy.grounded = false;
        flesh_guy.vel = sim::FxVec2::zero();
        flesh_guy.acc = sim::FxVec2::zero();
    }

    bool jumped_this_frame = false;
    if (flesh_guy.IsClimbing() && climb_tile.has_value()) {
        SnapToClimbTile(flesh_guy, *climb_tile);
        flesh_guy.grounded = false;
        flesh_guy.vel.x = sim::Scalar::zero();
        flesh_guy.acc.x = sim::Scalar::zero();
        flesh_guy.acc.y = sim::Scalar::zero();
        if (control.up && !control.down) {
            flesh_guy.vel.y = -sim::ToSimScalar(kClimbSpeed);
        } else if (control.down && !control.up) {
            flesh_guy.vel.y = sim::ToSimScalar(kClimbSpeed);
        } else {
            flesh_guy.vel.y = sim::Scalar::zero();
        }
        if (control.jump_pressed) {
            SetMovementFlag(flesh_guy, EntMovementFlag::Climbing, false);
            flesh_guy.vel.y = control.down ? sim::Scalar::zero() : -sim::ToSimScalar(kJumpImpulse);
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            jumped_this_frame = !control.down;
            (void)PlayEntCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        }
    }

    const std::optional<Side> wall_slide_side = flesh_guy.IsClimbing()
                                                          ? std::nullopt
                                                          : GetWallSlideSide(flesh_guy, control, state, graphics);
    if (wall_slide_side.has_value()) {
        RefreshWallSlideCoyote(flesh_guy, *wall_slide_side);
    }

    if (!flesh_guy.IsClimbing() && flesh_guy.condition == EntCondition::Normal && control.jump_pressed) {
        const std::optional<Side> wall_jump_side = wall_slide_side.has_value()
                                                            ? wall_slide_side
                                                            : GetWallSlideCoyoteSide(flesh_guy);
        if (wall_jump_side.has_value()) {
            flesh_guy.vel.y = -sim::ToSimScalar(kJumpImpulse);
            flesh_guy.vel.x = *wall_jump_side == Side::Left
                                 ? sim::ToSimScalar(kWallJumpHorizontalSpeed)
                                 : -sim::ToSimScalar(kWallJumpHorizontalSpeed);
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            ClearWallSlideCoyote(flesh_guy);
            jumped_this_frame = true;
            flesh_guy.jump_delay_frame_count = player::kJumpDelayFrames;
            (void)PlayEntCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        } else if ((flesh_guy.grounded && flesh_guy.jump_delay_frame_count == 0) || flesh_guy.coyote_time > 0) {
            flesh_guy.vel.y = -sim::ToSimScalar(kJumpImpulse);
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            jumped_this_frame = true;
            flesh_guy.jump_delay_frame_count = player::kJumpDelayFrames;
            (void)PlayEntCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        }
    }
    flesh_guy.jumped_this_frame = jumped_this_frame;

    const bool wall_sliding = wall_slide_side.has_value() && !jumped_this_frame;
    UpdateWallSlideState(flesh_guy, wall_sliding ? wall_slide_side : std::nullopt);

    if (flesh_guy.jump_delay_frame_count > 0) {
        flesh_guy.jump_delay_frame_count -= 1;
    }

    if (!flesh_guy.grounded && !flesh_guy.IsClimbing()) {
        flesh_guy.acc.y += state.stage.gravity *
            sim::ToSimScalar(wall_sliding ? kWallSlideGravityScale : kGravityScale);
    }

    common::PrePartialEulerStep(ent_idx, state, dt);
    if (wall_sliding && flesh_guy.vel.y > sim::ToSimScalar(kWallSlideMaxFallSpeed)) {
        flesh_guy.vel.y = sim::ToSimScalar(kWallSlideMaxFallSpeed);
    }
    flesh_guy.vel.x = gfxp::clamp(flesh_guy.vel.x,
                                  -sim::ToSimScalar(kMaxHorizontalSpeed),
                                  sim::ToSimScalar(kMaxHorizontalSpeed));
    flesh_guy.vel.y = gfxp::clamp(flesh_guy.vel.y,
                                  -sim::ToSimScalar(kMaxRiseSpeed),
                                  sim::ToSimScalar(kMaxFallSpeed));

    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    if (flesh_guy.grounded) {
        UpdateWallSlideState(flesh_guy, std::nullopt);
    }
    common::ApplySpecGroundFriction(ent_idx, state);
    MaybeSpawnTouchedMeatSlime(flesh_guy, state);
    SetFleshGuyAnim(flesh_guy, (!flesh_guy.grounded && wall_sliding) ? wall_slide_side : std::nullopt);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::flesh_guy
