#include "entities/flesh_guy.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "entities/player.hpp"
#include "frame_data_id.hpp"
#include "particles/particle_archetypes.hpp"
#include "particles/scripted_particle.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace splonks::entities::flesh_guy {

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
constexpr float kWalkAnimationVelocityEpsilon = 0.08F;
constexpr float kMeatTileTopperHeight = 7.0F;
constexpr float kMeatTileTopperYOffset = -1.0F;
constexpr float kSideMeatTileInset = (kMeatTileTopperHeight * 0.5F) - 1.0F;
constexpr float kClimbSpeed = 2.1F;
constexpr float kClimbSnapSpeed = 1.0F;

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

int FloorToTileCoord(float value) {
    return static_cast<int>(std::floor(value / static_cast<float>(kTileSize)));
}

IVec2 WorldPosToUnwrappedTileCoord(const Vec2& world_pos) {
    return IVec2::New(FloorToTileCoord(world_pos.x), FloorToTileCoord(world_pos.y));
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
    const Vec2& world_pos
) {
    return QueryCollidableTileOrBorderSurface(stage, WorldPosToUnwrappedTileCoord(world_pos));
}

bool IsClimbableTileQuery(const std::optional<WorldTileQueryResult>& tile_query) {
    return tile_query.has_value() && tile_query->tile != nullptr &&
           GetTileArchetype(*tile_query->tile).climbable;
}

std::optional<IVec2> GetClimbTile(const Entity& entity, const State& state) {
    const Vec2 center = entity.GetCenter();
    const float horizontal_offset = std::min(2.5F, std::max(0.0F, (entity.size.x * 0.5F) - 1.0F));
    const std::array<Vec2, 3> probes = {{
        Vec2::New(center.x - horizontal_offset, center.y),
        center,
        Vec2::New(center.x + horizontal_offset, center.y),
    }};

    for (const Vec2& probe : probes) {
        const std::optional<WorldTileQueryResult> tile_query = QueryTileAtWorldPos(state.stage, ToIVec2(probe));
        if (IsClimbableTileQuery(tile_query)) {
            return tile_query->tile_pos;
        }
    }
    return std::nullopt;
}

void SnapToClimbTile(Entity& entity, const IVec2& tile_pos) {
    Vec2 center = entity.GetCenter();
    const float target_x = static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2));
    const float delta = std::clamp(target_x - center.x, -kClimbSnapSpeed, kClimbSnapSpeed);
    center.x += delta;
    entity.SetCenter(center);
}

std::optional<LeftOrRight> GetWallSlideSide(
    const Entity& entity,
    const controls::ControlIntent& control,
    const State& state,
    const Graphics& graphics
) {
    if (entity.grounded || entity.condition != EntityCondition::Normal) {
        return std::nullopt;
    }

    const AABB aabb = entity.GetAABB();
    const auto side_blocked = [&](LeftOrRight side) {
        const bool left = side == LeftOrRight::Left;
        const float probe_x = left ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
        const AABB probe = AABB::New(
            Vec2::New(probe_x, aabb.tl.y + 1.0F),
            Vec2::New(probe_x, aabb.br.y - 1.0F)
        );
        return AabbHitsBlockingWorldGeometryOrImpassableEntities(state, graphics, probe, entity.vid);
    };

    if (control.left && !control.right && side_blocked(LeftOrRight::Left)) {
        return LeftOrRight::Left;
    }
    if (control.right && !control.left && side_blocked(LeftOrRight::Right)) {
        return LeftOrRight::Right;
    }
    return std::nullopt;
}

std::optional<MeatSlimeSurface> GetGroundSurface(const Entity& entity, const State& state) {
    if (!entity.grounded) {
        return std::nullopt;
    }

    const AABB aabb = entity.GetAABB();
    const Vec2 center_support_world = Vec2::New(entity.GetCenter().x, aabb.br.y + 1.0F);
    if (const std::optional<MeatSlimeSurface> center_surface =
            QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_support_world)) {
        return center_surface;
    }

    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, entity.GetFeet())) {
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

void SpawnMeatSlime(State& state, const Vec2& particle_center, float rotation = 0.0F) {
    ScriptedParticle particle = MakeScriptedParticle(scripted_particle_archetype_ids::MeatTileTopper, particle_center);
    if (!particle.active) {
        return;
    }
    particle.rot = rotation;
    state.particles.Add(std::move(particle));
}

Vec2 GetTopMeatSlimeCenter(const IVec2& tile_pos) {
    return Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize)) +
            (kMeatTileTopperHeight * 0.5F) + kMeatTileTopperYOffset
    );
}

Vec2 GetTopMeatSlimeCenter(const MeatSlimeSurface& surface, const Stage& stage) {
    (void)stage;
    return GetTopMeatSlimeCenter(surface.tile_pos);
}

Vec2 GetBottomMeatSlimeCenter(const IVec2& tile_pos) {
    return Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2)),
        static_cast<float>((tile_pos.y + 1) * static_cast<int>(kTileSize)) -
            (kMeatTileTopperHeight * 0.5F) - kMeatTileTopperYOffset
    );
}

Vec2 GetBottomMeatSlimeCenter(const MeatSlimeSurface& surface, const Stage& stage) {
    (void)stage;
    return GetBottomMeatSlimeCenter(surface.tile_pos);
}

Vec2 GetSideMeatSlimeCenter(const IVec2& tile_pos, LeftOrRight tile_side) {
    const float tile_left = static_cast<float>(tile_pos.x * static_cast<int>(kTileSize));
    const float tile_top = static_cast<float>(tile_pos.y * static_cast<int>(kTileSize));
    const float x = tile_side == LeftOrRight::Left
                        ? tile_left + kSideMeatTileInset
                        : tile_left + static_cast<float>(kTileSize) - kSideMeatTileInset;
    return Vec2::New(x, tile_top + static_cast<float>(kTileSize / 2));
}

Vec2 GetSideMeatSlimeCenter(const MeatSlimeSurface& surface, LeftOrRight tile_side, const Stage& stage) {
    (void)stage;
    return GetSideMeatSlimeCenter(surface.tile_pos, tile_side);
}

void MaybeSpawnTopMeatSlime(Entity& entity, State& state) {
    const std::optional<MeatSlimeSurface> ground_surface = GetGroundSurface(entity, state);
    if (!ground_surface.has_value()) {
        entity.point_label_a = PointLabel::None;
        return;
    }

    if (entity.point_label_a == PointLabel::Target && entity.point_a == ground_surface->key) {
        return;
    }

    entity.point_a = ground_surface->key;
    entity.point_label_a = PointLabel::Target;
    SpawnMeatSlime(state, GetTopMeatSlimeCenter(*ground_surface, state.stage));
}

std::optional<MeatSlimeSurface> GetCeilingSurface(const Entity& entity, const State& state) {
    const AABB aabb = entity.GetAABB();
    const float probe_y = aabb.tl.y - 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(aabb.tl.x + 1.0F, probe_y),
        Vec2::New(aabb.br.x - 1.0F, probe_y)
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

    const Vec2 center_probe = Vec2::New(entity.GetCenter().x, probe_y);
    return QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_probe);
}

void MaybeSpawnBottomMeatSlime(Entity& entity, State& state) {
    const std::optional<MeatSlimeSurface> ceiling_surface = GetCeilingSurface(entity, state);
    if (!ceiling_surface.has_value()) {
        entity.point_label_d = PointLabel::None;
        return;
    }

    if (entity.point_label_d == PointLabel::Target && entity.point_d == ceiling_surface->key) {
        return;
    }

    entity.point_d = ceiling_surface->key;
    entity.point_label_d = PointLabel::Target;
    SpawnMeatSlime(state, GetBottomMeatSlimeCenter(*ceiling_surface, state.stage), 180.0F);
}

std::optional<MeatSlimeSurface> GetSideSurface(const Entity& entity, const State& state, LeftOrRight side) {
    const AABB aabb = entity.GetAABB();
    const float probe_x = side == LeftOrRight::Left ? aabb.tl.x - 1.0F : aabb.br.x + 1.0F;
    const AABB probe = AABB::New(
        Vec2::New(probe_x, aabb.tl.y + 1.0F),
        Vec2::New(probe_x, aabb.br.y - 1.0F)
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

    const Vec2 center_probe = Vec2::New(probe_x, entity.GetCenter().y);
    return QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, center_probe);
}

void MaybeSpawnSideMeatSlime(Entity& entity, State& state, LeftOrRight side) {
    const std::optional<MeatSlimeSurface> side_surface = GetSideSurface(entity, state, side);
    PointLabel& label = side == LeftOrRight::Left ? entity.point_label_b : entity.point_label_c;
    IVec2& cached_tile = side == LeftOrRight::Left ? entity.point_b : entity.point_c;
    if (!side_surface.has_value()) {
        label = PointLabel::None;
        return;
    }

    if (label == PointLabel::Target && cached_tile == side_surface->key) {
        return;
    }

    cached_tile = side_surface->key;
    label = PointLabel::Target;
    const LeftOrRight tile_side = side == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
    const float rotation = side == LeftOrRight::Left ? 90.0F : -90.0F;
    SpawnMeatSlime(state, GetSideMeatSlimeCenter(*side_surface, tile_side, state.stage), rotation);
}

void MaybeSpawnTouchedMeatSlime(Entity& entity, State& state) {
    MaybeSpawnTopMeatSlime(entity, state);
    MaybeSpawnBottomMeatSlime(entity, state);
    MaybeSpawnSideMeatSlime(entity, state, LeftOrRight::Left);
    MaybeSpawnSideMeatSlime(entity, state, LeftOrRight::Right);
}

void SetFleshGuyAnimation(Entity& entity, std::optional<LeftOrRight> wall_slide_side = std::nullopt) {
    if (entity.condition == EntityCondition::Dead) {
        return;
    }

    if (wall_slide_side.has_value()) {
        if (entity.frame_data_animator.animation_id != frame_data_ids::FleshGuyWalk) {
            SetAnimation(entity, frame_data_ids::FleshGuyWalk);
            entity.frame_data_animator.SetForcedFrame(0);
        }
        entity.facing = *wall_slide_side;
        entity.frame_data_animator.animate = false;
        entity.frame_data_animator.loop = false;
        return;
    }

    if (entity.grounded && std::abs(entity.vel.x) > kWalkAnimationVelocityEpsilon) {
        if (entity.frame_data_animator.animation_id != frame_data_ids::FleshGuyWalk) {
            entity.frame_data_animator.PlayLoop(frame_data_ids::FleshGuyWalk);
        }
        entity.frame_data_animator.animate = true;
        entity.frame_data_animator.loop = true;
        return;
    }

    if (entity.frame_data_animator.animation_id != frame_data_ids::FleshGuy) {
        SetAnimation(entity, frame_data_ids::FleshGuy);
        entity.frame_data_animator.SetForcedFrame(0);
    }
    entity.frame_data_animator.animate = false;
    entity.frame_data_animator.loop = false;
}

void UpdateWallSlideState(
    Entity& entity,
    const std::optional<LeftOrRight>& wall_slide_side
) {
    if (wall_slide_side.has_value()) {
        entity.hang_side = wall_slide_side;
    } else if (entity.grounded || entity.coyote_time == 0) {
        entity.hang_side.reset();
    }
    SetMovementFlag(entity, EntityMovementFlag::Hanging, wall_slide_side.has_value());
}

void RefreshWallSlideCoyote(Entity& entity, LeftOrRight side) {
    entity.coyote_time = std::max(entity.coyote_time, player::kCoyoteTimeFrames);
    entity.hang_side = side;
}

std::optional<LeftOrRight> GetWallSlideCoyoteSide(const Entity& entity) {
    if (entity.coyote_time == 0 || !entity.hang_side.has_value()) {
        return std::nullopt;
    }
    return entity.hang_side;
}

void ClearWallSlideCoyote(Entity& entity) {
    entity.coyote_time = 0;
    entity.hang_side.reset();
    SetMovementFlag(entity, EntityMovementFlag::Hanging, false);
}

void StepTravelSoundFleshGuy(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.travel_sound_countdown -= entity.dist_traveled_this_frame;

    if ((!entity.grounded && !entity.IsClimbing()) || entity.dist_traveled_this_frame <= 0.0F) {
        return;
    }

    if (entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;
        const AudioAssetId sound = entity.travel_sound == TravelSound::One
                                       ? audio_asset_ids::FleshGuyStep0
                                       : audio_asset_ids::FleshGuyStep1;
        (void)PlayEntitySoundEmitter(state, entity, sound);
        entity.IncTravelSound();
    }
}

} // namespace

extern const EntityArchetype kFleshGuyArchetype{
    .type_ = EntityType::FleshGuy,
    .size = Vec2::New(8.0F, 9.0F),
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
    .throw_velocity_scale = 0.5F,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Right,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .collide_sound = audio_asset_ids::FleshGuyImpact1,
    .death_sound = audio_asset_ids::FleshGuyImpact0,
    .on_death = OnDeathAsFleshGuy,
    .control_logic = ControlEntityAsFleshGuy,
    .step_logic = StepEntityLogicAsFleshGuy,
    .step_physics = StepEntityPhysicsAsFleshGuy,
    .alignment = Alignment::Ally,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::FleshGuy),
};

void OnDeathAsFleshGuy(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& flesh_guy = state.entity_manager.entities[entity_idx];
    if (const std::optional<MeatSlimeSurface> ground_surface = GetGroundSurface(flesh_guy, state)) {
        SpawnMeatSlime(state, GetTopMeatSlimeCenter(*ground_surface, state.stage));
    } else if (const std::optional<MeatSlimeSurface> center_surface =
                   QueryCollidableTileOrBorderSurfaceAtWorldPos(state.stage, flesh_guy.GetCenter())) {
        SpawnMeatSlime(state, GetTopMeatSlimeCenter(*center_surface, state.stage));
    }
    state.entity_manager.SetInactive(entity_idx);
}

void ControlEntityAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& flesh_guy = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEntity(flesh_guy, state);
    if (flesh_guy.condition != EntityCondition::Normal) {
        return;
    }

    const bool climbing = flesh_guy.IsClimbing();
    if (climbing) {
        flesh_guy.acc.x = 0.0F;
        flesh_guy.vel.x = 0.0F;
    } else if (control.left && !control.right) {
        common::AccelerateHorizontallyTowardSpeed(
            flesh_guy,
            state,
            flesh_guy.grounded ? -kGroundTargetSpeed : -kAirTargetSpeed,
            flesh_guy.grounded ? kGroundMoveAcc : kAirMoveAcc
        );
        flesh_guy.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        common::AccelerateHorizontallyTowardSpeed(
            flesh_guy,
            state,
            flesh_guy.grounded ? kGroundTargetSpeed : kAirTargetSpeed,
            flesh_guy.grounded ? kGroundMoveAcc : kAirMoveAcc
        );
        flesh_guy.facing = LeftOrRight::Right;
    } else if (flesh_guy.grounded) {
        common::DecelerateHorizontallyToStop(flesh_guy, kGroundNoInputDecel);
    } else {
        flesh_guy.vel.x *= kAirNoInputDamping;
    }

    if (control.stop) {
        flesh_guy.acc = Vec2::New(0.0F, 0.0F);
        flesh_guy.vel = Vec2::New(0.0F, 0.0F);
    }
}

void StepEntityLogicAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& flesh_guy = state.entity_manager.entities[entity_idx];
    if (flesh_guy.condition == EntityCondition::Dead) {
        return;
    }

    StepTravelSoundFleshGuy(entity_idx, state);
    common::CleanupInactiveCarryReferences(entity_idx, state);

    const bool loss_of_control = flesh_guy.condition == EntityCondition::Stunned;
    const controls::ControlIntent control = controls::GetControlIntentForEntity(flesh_guy, state);
    const bool walking =
        !loss_of_control &&
        flesh_guy.grounded &&
        (control.left != control.right) &&
        std::abs(flesh_guy.vel.x) > kWalkAnimationVelocityEpsilon;
    SetMovementFlag(flesh_guy, EntityMovementFlag::Walking, walking);
    SetMovementFlag(flesh_guy, EntityMovementFlag::Running, false);

    SetFleshGuyAnimation(flesh_guy, flesh_guy.grounded ? std::nullopt : flesh_guy.hang_side);
    common::UpdateCarryAndBackItems(entity_idx, state, graphics, audio);

    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 0, control.bomb_pressed);
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 1, control.rope_pressed);
        common::TryPushBlocks(entity_idx, state, graphics);
    }
}

void StepEntityPhysicsAsFleshGuy(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    common::GroundedCheck(entity_idx, state, audio, true, true);

    Entity& flesh_guy = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEntity(flesh_guy, state);
    const std::optional<IVec2> climb_tile = GetClimbTile(flesh_guy, state);
    if (flesh_guy.condition != EntityCondition::Normal || !climb_tile.has_value()) {
        SetMovementFlag(flesh_guy, EntityMovementFlag::Climbing, false);
    } else if (!flesh_guy.IsClimbing() && (control.up || control.down)) {
        SetMovementFlag(flesh_guy, EntityMovementFlag::Climbing, true);
        flesh_guy.grounded = false;
        flesh_guy.vel = Vec2::New(0.0F, 0.0F);
        flesh_guy.acc = Vec2::New(0.0F, 0.0F);
    }

    bool jumped_this_frame = false;
    if (flesh_guy.IsClimbing() && climb_tile.has_value()) {
        SnapToClimbTile(flesh_guy, *climb_tile);
        flesh_guy.grounded = false;
        flesh_guy.vel.x = 0.0F;
        flesh_guy.acc.x = 0.0F;
        flesh_guy.acc.y = 0.0F;
        if (control.up && !control.down) {
            flesh_guy.vel.y = -kClimbSpeed;
        } else if (control.down && !control.up) {
            flesh_guy.vel.y = kClimbSpeed;
        } else {
            flesh_guy.vel.y = 0.0F;
        }
        if (control.jump_pressed) {
            SetMovementFlag(flesh_guy, EntityMovementFlag::Climbing, false);
            flesh_guy.vel.y = control.down ? 0.0F : -kJumpImpulse;
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            jumped_this_frame = !control.down;
            (void)PlayEntityCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        }
    }

    const std::optional<LeftOrRight> wall_slide_side = flesh_guy.IsClimbing()
                                                          ? std::nullopt
                                                          : GetWallSlideSide(flesh_guy, control, state, graphics);
    if (wall_slide_side.has_value()) {
        RefreshWallSlideCoyote(flesh_guy, *wall_slide_side);
    }

    if (!flesh_guy.IsClimbing() && flesh_guy.condition == EntityCondition::Normal && control.jump_pressed) {
        const std::optional<LeftOrRight> wall_jump_side = wall_slide_side.has_value()
                                                            ? wall_slide_side
                                                            : GetWallSlideCoyoteSide(flesh_guy);
        if (wall_jump_side.has_value()) {
            flesh_guy.vel.y = -kJumpImpulse;
            flesh_guy.vel.x = *wall_jump_side == LeftOrRight::Left
                                 ? kWallJumpHorizontalSpeed
                                 : -kWallJumpHorizontalSpeed;
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            ClearWallSlideCoyote(flesh_guy);
            jumped_this_frame = true;
            flesh_guy.jump_delay_frame_count = player::kJumpDelayFrames;
            (void)PlayEntityCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        } else if ((flesh_guy.grounded && flesh_guy.jump_delay_frame_count == 0) || flesh_guy.coyote_time > 0) {
            flesh_guy.vel.y = -kJumpImpulse;
            flesh_guy.grounded = false;
            flesh_guy.coyote_time = 0;
            jumped_this_frame = true;
            flesh_guy.jump_delay_frame_count = player::kJumpDelayFrames;
            (void)PlayEntityCenterSoundEmitter(state, flesh_guy, audio_asset_ids::FleshmanJump);
        }
    }
    flesh_guy.jumped_this_frame = jumped_this_frame;

    const bool wall_sliding = wall_slide_side.has_value() && !jumped_this_frame;
    UpdateWallSlideState(flesh_guy, wall_sliding ? wall_slide_side : std::nullopt);

    if (flesh_guy.jump_delay_frame_count > 0) {
        flesh_guy.jump_delay_frame_count -= 1;
    }

    if (!flesh_guy.grounded && !flesh_guy.IsClimbing()) {
        flesh_guy.acc.y += state.stage.gravity * (wall_sliding ? kWallSlideGravityScale : kGravityScale);
    }

    common::PrePartialEulerStep(entity_idx, state, dt);
    if (wall_sliding && flesh_guy.vel.y > kWallSlideMaxFallSpeed) {
        flesh_guy.vel.y = kWallSlideMaxFallSpeed;
    }
    flesh_guy.vel.x = std::clamp(flesh_guy.vel.x, -kMaxHorizontalSpeed, kMaxHorizontalSpeed);
    flesh_guy.vel.y = std::clamp(flesh_guy.vel.y, -kMaxRiseSpeed, kMaxFallSpeed);

    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    if (flesh_guy.grounded) {
        UpdateWallSlideState(flesh_guy, std::nullopt);
    }
    common::ApplyArchetypeGroundFriction(entity_idx, state);
    MaybeSpawnTouchedMeatSlime(flesh_guy, state);
    SetFleshGuyAnimation(flesh_guy, (!flesh_guy.grounded && wall_sliding) ? wall_slide_side : std::nullopt);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::flesh_guy
