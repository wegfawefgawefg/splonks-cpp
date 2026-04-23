#include "entities/teleporter.hpp"

#include "audio_asset_id.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace splonks::entities::teleporter {

namespace {

constexpr int kTeleportCardinalMinTiles = 4;
constexpr int kTeleportCardinalMaxTiles = 8;
constexpr int kTeleportDiagonalMinTiles = 3;
constexpr int kTeleportDiagonalMaxTiles = 6;
constexpr unsigned int kTelefragDamage = 9999;

enum class TeleportProbeBlockReason {
    None,
    World,
    BlockingEntity,
};

struct TeleportAim {
    IVec2 direction = IVec2::New(1, 0);
    int min_tiles = kTeleportCardinalMinTiles;
    int max_tiles = kTeleportCardinalMaxTiles;
};

struct TeleportProbeCandidate {
    int distance_tiles = 0;
    IVec2 tile_pos = IVec2::New(0, 0);
    Vec2 destination_center = Vec2::New(0.0F, 0.0F);
    AABB destination_aabb = AABB::New(Vec2::New(0.0F, 0.0F), Vec2::New(0.0F, 0.0F));
    TeleportProbeBlockReason block_reason = TeleportProbeBlockReason::None;
    std::vector<VID> telefrag_vids;
    std::vector<VID> splat_vids;
};

bool IsDiagonalDirection(const IVec2& direction) {
    return direction.x != 0 && direction.y != 0;
}

AABB TileAabbForTilePos(const IVec2& tile_pos) {
    const Vec2 tile_top_left = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
    );
    return AABB::New(tile_top_left, tile_top_left + Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize)));
}

Vec2 TileCenterForTilePos(const IVec2& tile_pos) {
    const AABB tile_aabb = TileAabbForTilePos(tile_pos);
    return (tile_aabb.tl + tile_aabb.br) * 0.5F;
}


Vec2 GetTeleportAxis(const IVec2& direction) {
    const Vec2 axis = Vec2::New(static_cast<float>(direction.x), static_cast<float>(direction.y));
    const float length = Length(axis);
    if (length <= 0.0F) {
        return Vec2::New(1.0F, 0.0F);
    }
    return axis / length;
}

Vec2 GetTeleportOrtho(const Vec2& axis) {
    return Vec2::New(-axis.y, axis.x);
}

bool HasTeleportAnimation(const Graphics& graphics, FrameDataId animation_id) {
    return graphics.frame_data_db.FindFrame(animation_id, 0) != nullptr;
}

FrameDataId GetTeleporterBackpackAnimation(const Entity& teleporter, const Entity* holder, const Graphics& graphics) {
    if (teleporter.attachment_mode == AttachmentMode::Back) {
        if (holder != nullptr) {
            if (holder->IsHanging() && HasTeleportAnimation(graphics, frame_data_ids::TeleporterBackpackSide)) {
                return frame_data_ids::TeleporterBackpackSide;
            }
            if (holder->IsClimbing() && HasTeleportAnimation(graphics, frame_data_ids::TeleporterBackpackBack)) {
                return frame_data_ids::TeleporterBackpackBack;
            }
        }
        if (HasTeleportAnimation(graphics, frame_data_ids::TeleporterBackpackBack)) {
            return frame_data_ids::TeleporterBackpackBack;
        }
    }
    if (teleporter.held_by_vid.has_value() && HasTeleportAnimation(graphics, frame_data_ids::TeleporterBackpackSide)) {
        return frame_data_ids::TeleporterBackpackSide;
    }
    if (HasTeleportAnimation(graphics, frame_data_ids::TeleporterBackpack)) {
        return frame_data_ids::TeleporterBackpack;
    }
    return frame_data_ids::Teleporter;
}

TeleportAim GetTeleportAim(const Entity& teleporter, const Entity& holder, const State& state) {
    const controls::ControlIntent intent = controls::GetControlIntentForEntity(holder, state);

    const int dx = (intent.right ? 1 : 0) - (intent.left ? 1 : 0);
    const int dy = (intent.down ? 1 : 0) - (intent.up ? 1 : 0);
    IVec2 direction = IVec2::New(dx, dy);
    if (direction.x == 0 && direction.y == 0) {
        direction = teleporter.facing == LeftOrRight::Left ? IVec2::New(-1, 0) : IVec2::New(1, 0);
    }

    if (IsDiagonalDirection(direction)) {
        return TeleportAim{
            .direction = direction,
            .min_tiles = kTeleportDiagonalMinTiles,
            .max_tiles = kTeleportDiagonalMaxTiles,
        };
    }

    return TeleportAim{
        .direction = direction,
        .min_tiles = kTeleportCardinalMinTiles,
        .max_tiles = kTeleportCardinalMaxTiles,
    };
}

Entity BuildTeleporterProbeEntity(
    const Entity& holder,
    const Graphics& graphics,
    const Vec2& destination_center
) {
    Entity probe = holder;
    common::SetVisualCenterForEntity(probe, graphics, destination_center);
    return probe;
}

bool DoesEntityBlockTeleportDestination(const Entity& entity) {
    if (!entity.active || entity.condition == EntityCondition::Dead) {
        return false;
    }
    if (entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None) {
        return true;
    }
    if (!entity.can_collide) {
        return false;
    }
    if (entity.impassable) {
        return true;
    }
    return !entity.can_be_hit;
}

bool IsAttackKillableActor(const Entity& entity) {
    return (entity.can_be_stunned || entity.hurt_on_contact || entity.alignment == Alignment::Enemy) &&
           common::CanEntityTakeDamageType(entity, DamageType::Attack);
}

bool IsAttackKillableBreakable(const Entity& entity) {
    return entity.on_death != nullptr && common::CanEntityTakeDamageType(entity, DamageType::Attack);
}

bool IsAttackReactiveProp(const Entity& entity) {
    return entity.on_damage != nullptr;
}

bool CanTelefragEntity(const Entity& entity) {
    if (!entity.active || !entity.can_collide || entity.condition == EntityCondition::Dead) {
        return false;
    }
    if (entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None) {
        return false;
    }
    if (entity.impassable || !entity.can_be_hit) {
        return false;
    }

    return IsAttackKillableActor(entity) ||
           IsAttackKillableBreakable(entity) ||
           IsAttackReactiveProp(entity);
}

bool CanSplatDeadEntity(const Entity& entity) {
    if (!entity.active || entity.condition != EntityCondition::Dead) {
        return false;
    }
    if (entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None) {
        return false;
    }
    if (!entity.can_collide || entity.impassable) {
        return false;
    }
    return true;
}

bool DoesProbeOverlapEntity(
    const AABB& probe_aabb,
    const Vec2& probe_center,
    const Entity& other,
    const State& state,
    const Graphics& graphics
) {
    const AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        probe_center,
        common::GetContactAabbForEntity(other, graphics)
    );
    return AabbsIntersect(probe_aabb, other_aabb);
}

TeleportProbeCandidate EvaluateTeleportProbeCandidate(
    const Entity& holder,
    const TeleportAim& aim,
    std::size_t holder_idx,
    std::size_t teleporter_idx,
    int distance_tiles,
    State& state,
    const Graphics& graphics
) {
    const Vec2 holder_visual_center = common::GetVisualCenterForEntity(holder, graphics, holder.GetCenter());
    const IVec2 holder_tile = state.stage.GetTileCoordAtWc(ToIVec2(holder_visual_center));
    const IVec2 raw_target_tile = holder_tile + IVec2::New(aim.direction.x * distance_tiles, aim.direction.y * distance_tiles);
    const IVec2 target_tile = state.stage.WrapTileCoord(raw_target_tile);
    const Vec2 destination_center = TileCenterForTilePos(target_tile);

    TeleportProbeCandidate candidate{
        .distance_tiles = distance_tiles,
        .tile_pos = target_tile,
        .destination_center = destination_center,
        .destination_aabb = AABB::New(Vec2::New(0.0F, 0.0F), Vec2::New(0.0F, 0.0F)),
        .block_reason = TeleportProbeBlockReason::None,
        .telefrag_vids = {},
        .splat_vids = {},
    };

    const Entity probe = BuildTeleporterProbeEntity(holder, graphics, destination_center);
    candidate.destination_aabb = common::GetContactAabbForEntity(probe, graphics);

    if (AabbHitsBlockingWorldGeometryOrImpassableEntities(
            state,
            graphics,
            candidate.destination_aabb,
            holder.vid
        )) {
        candidate.block_reason = TeleportProbeBlockReason::World;
        return candidate;
    }

    for (const VID& other_vid : QueryEntitiesInAabb(state, candidate.destination_aabb, holder.vid)) {
        if (other_vid.id == teleporter_idx || other_vid.id == holder_idx) {
            continue;
        }

        const Entity* const other = state.entity_manager.GetEntity(other_vid);
        if (other == nullptr || !DoesProbeOverlapEntity(candidate.destination_aabb, destination_center, *other, state, graphics)) {
            continue;
        }
        if (CanSplatDeadEntity(*other)) {
            candidate.splat_vids.push_back(other_vid);
            continue;
        }
        if (DoesEntityBlockTeleportDestination(*other)) {
            candidate.block_reason = TeleportProbeBlockReason::BlockingEntity;
            candidate.telefrag_vids.clear();
            candidate.splat_vids.clear();
            return candidate;
        }
        if (CanTelefragEntity(*other)) {
            candidate.telefrag_vids.push_back(other_vid);
        }
    }

    return candidate;
}

std::vector<TeleportProbeCandidate> BuildTeleportProbeCandidates(
    const Entity& holder,
    const TeleportAim& aim,
    std::size_t holder_idx,
    std::size_t teleporter_idx,
    State& state,
    const Graphics& graphics
) {
    std::vector<TeleportProbeCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(aim.max_tiles - aim.min_tiles + 1));
    for (int distance_tiles = aim.min_tiles; distance_tiles <= aim.max_tiles; ++distance_tiles) {
        candidates.push_back(EvaluateTeleportProbeCandidate(
            holder,
            aim,
            holder_idx,
            teleporter_idx,
            distance_tiles,
            state,
            graphics
        ));
    }
    return candidates;
}

const TeleportProbeCandidate* FindFirstValidTeleportProbeCandidate(const std::vector<TeleportProbeCandidate>& candidates) {
    for (const TeleportProbeCandidate& candidate : candidates) {
        if (candidate.block_reason == TeleportProbeBlockReason::None) {
            return &candidate;
        }
    }
    return nullptr;
}

const TeleportProbeCandidate* FindFirstBlockedTeleportProbeCandidate(const std::vector<TeleportProbeCandidate>& candidates) {
    for (const TeleportProbeCandidate& candidate : candidates) {
        if (candidate.block_reason != TeleportProbeBlockReason::None) {
            return &candidate;
        }
    }
    return nullptr;
}

void SpawnTelefragPhaseParticle(
    const Entity& entity,
    const Graphics& graphics,
    const Vec2& start_offset,
    const Vec2& velocity,
    float tint_r,
    float tint_g,
    float tint_b,
    State& state
) {
    const FrameData* const frame_data = common::GetCurrentFrameDataForEntity(entity, graphics);
    if (frame_data == nullptr) {
        return;
    }

    SpriteParticle particle{};
    particle.counter = 32;
    particle.draw_layer = entity.draw_layer;
    particle.pos = common::GetVisualCenterForEntity(entity, graphics, entity.GetCenter()) + start_offset;
    particle.size = Vec2::New(
        static_cast<float>(frame_data->sample_rect.w),
        static_cast<float>(frame_data->sample_rect.h)
    ) * entity.frame_data_animator.scale;
    particle.rot = entity.rotation;
    particle.alpha = 0.85F;
    particle.tint_r = tint_r;
    particle.tint_g = tint_g;
    particle.tint_b = tint_b;
    particle.horizontal_flip = entity.facing == LeftOrRight::Right;
    particle.vel = velocity;
    particle.alpha_vel = -0.0275F;
    particle.frame_data_animator = entity.frame_data_animator;
    particle.frame_data_animator.animate = false;
    state.particles.Add(std::move(particle));
}

void SpawnTelefragSplitEffect(const Entity& entity, const Graphics& graphics, const IVec2& direction, State& state) {
    const Vec2 axis = GetTeleportAxis(direction);
    const Vec2 ortho = GetTeleportOrtho(axis);
    SpawnTelefragPhaseParticle(entity, graphics, Vec2::New(0.0F, 0.0F), (axis * -0.3F) - (ortho * 0.0625F), 1.0F, 0.20F, 0.20F, state);
    SpawnTelefragPhaseParticle(entity, graphics, Vec2::New(0.0F, 0.0F), ortho * 0.0375F, 0.25F, 1.0F, 0.25F, state);
    SpawnTelefragPhaseParticle(entity, graphics, Vec2::New(0.0F, 0.0F), (axis * 0.3F) - (ortho * 0.0625F), 0.30F, 0.30F, 1.0F, state);
}

void SpawnTelefragMergeEffect(const Entity& entity, const Graphics& graphics, const IVec2& direction, State& state) {
    const Vec2 axis = GetTeleportAxis(direction);
    const Vec2 ortho = GetTeleportOrtho(axis);
    SpawnTelefragPhaseParticle(entity, graphics, axis * -3.0F, axis * 0.3F, 1.0F, 0.20F, 0.20F, state);
    SpawnTelefragPhaseParticle(entity, graphics, ortho * 2.0F, ortho * -0.0875F, 0.25F, 1.0F, 0.25F, state);
    SpawnTelefragPhaseParticle(entity, graphics, axis * 3.0F, axis * -0.3F, 0.30F, 0.30F, 1.0F, state);
}

void SplatTelefraggedEntity(std::size_t entity_idx, State& state, const Graphics& graphics) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active || entity.impassable) {
        return;
    }

    SpawnTelefragSplitEffect(entity, graphics, IVec2::New(1, 0), state);
    common::DropHeldItemFromEntity(entity, state);
    common::ReleaseEntityFromHolder(entity, state);
    entity.marked_for_destruction = true;
    state.entity_manager.SetInactive(entity_idx);
}

void ApplyTelefragToCandidate(const TeleportProbeCandidate& candidate, State& state, Audio& audio, const Graphics& graphics) {
    for (const VID& other_vid : candidate.splat_vids) {
        SplatTelefraggedEntity(other_vid.id, state, graphics);
    }

    for (const VID& other_vid : candidate.telefrag_vids) {
        if (other_vid.id >= state.entity_manager.entities.size()) {
            continue;
        }

        const common::DamageResult damage_result =
            common::TryDamageEntity(other_vid.id, state, audio, DamageType::Attack, kTelefragDamage);
        if (damage_result == common::DamageResult::Died) {
            SplatTelefraggedEntity(other_vid.id, state, graphics);
        }
    }
}

void MoveTeleportHolderToDestination(
    Entity& holder,
    std::size_t holder_idx,
    const Vec2& destination_center,
    State& state,
    const Graphics& graphics
) {
    common::SetVisualCenterForEntity(holder, graphics, destination_center);
    holder.pos = Vec2::New(std::round(holder.pos.x), std::round(holder.pos.y));
    holder.grounded = false;
    holder.hang_side.reset();
    SetMovementFlag(holder, EntityMovementFlag::Climbing, false);
    SetMovementFlag(holder, EntityMovementFlag::Hanging, false);
    holder.climb_detach_cooldown = 0;
    state.UpdateSidForEntity(holder_idx, graphics);
    common::SyncEntityAttachments(holder_idx, state, graphics);
}

void ApplyTeleportAreaShake(
    State& state,
    const Vec2& world_pos,
    float foreground_tile_amount,
    float background_tile_amount,
    float entity_amount,
    float radius_tiles
) {
    AddShake(
        state,
        world_pos,
        foreground_tile_amount,
        background_tile_amount,
        entity_amount,
        radius_tiles,
        std::nullopt
    );
}

void AddTeleporterDebugAnnotations(
    const Entity& teleporter,
    const Entity* holder,
    std::size_t holder_idx,
    std::size_t teleporter_idx,
    State& state,
    const Graphics& graphics
) {
    if (!state.debug_overlay.show_debug_annotations || holder == nullptr) {
        return;
    }

    const TeleportAim aim = GetTeleportAim(teleporter, *holder, state);
    const std::vector<TeleportProbeCandidate> candidates =
        BuildTeleportProbeCandidates(*holder, aim, holder_idx, teleporter_idx, state, graphics);
    const TeleportProbeCandidate* const chosen = FindFirstValidTeleportProbeCandidate(candidates);

    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = holder->GetCenter(),
        .text = "teleporter dir (" + std::to_string(aim.direction.x) + ", " + std::to_string(aim.direction.y) + ")",
        .color = DebugAnnotationColor{0, 255, 255, 255},
    });

    for (const TeleportProbeCandidate& candidate : candidates) {
        DebugAnnotationColor color{255, 0, 0, 255};
        if (&candidate == chosen) {
            color = DebugAnnotationColor{0, 255, 0, 255};
        } else if (candidate.block_reason == TeleportProbeBlockReason::None) {
            color = DebugAnnotationColor{255, 255, 0, 255};
        } else if (candidate.block_reason == TeleportProbeBlockReason::BlockingEntity) {
            color = DebugAnnotationColor{255, 0, 255, 255};
        }

        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = candidate.destination_aabb,
            .color = color,
        });

        std::string text = "tp " + std::to_string(candidate.distance_tiles) + " (" +
                           std::to_string(candidate.tile_pos.x) + ", " +
                           std::to_string(candidate.tile_pos.y) + ")";
        if (candidate.block_reason == TeleportProbeBlockReason::World) {
            text += " blocked";
        } else if (candidate.block_reason == TeleportProbeBlockReason::BlockingEntity) {
            text += " entity";
        } else {
            if (!candidate.telefrag_vids.empty()) {
                text += " telefrag";
            }
            if (!candidate.splat_vids.empty()) {
                text += candidate.telefrag_vids.empty() ? " splat" : "+splat";
            }
        }

        state.AddDebugLabelAnnotation(DebugLabelAnnotation{
            .world_pos = candidate.destination_center,
            .text = text,
            .color = color,
        });
    }
}

void KillTeleportHolder(std::size_t holder_idx, State& state, Audio& audio) {
    if (holder_idx >= state.entity_manager.entities.size()) {
        return;
    }
    Entity& holder = state.entity_manager.entities[holder_idx];
    holder.health = 0;
    common::DieIfDead(holder_idx, state, audio);
}

void CrushTeleportHolder(std::size_t holder_idx, State& state, Audio& audio) {
    if (holder_idx >= state.entity_manager.entities.size()) {
        return;
    }
    (void)common::TryDamageEntity(holder_idx, state, audio, DamageType::Crush, 1);
}

} // namespace

void OnUseAsTeleporter(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    Entity& teleporter = state.entity_manager.entities[entity_idx];
    if (!teleporter.use_state.pressed || !teleporter.held_by_vid.has_value()) {
        return;
    }

    Entity* const holder = state.entity_manager.GetEntityMut(*teleporter.held_by_vid);
    if (holder == nullptr || !holder->active) {
        return;
    }

    const std::size_t holder_idx = holder->vid.id;
    const TeleportAim aim = GetTeleportAim(teleporter, *holder, state);
    const std::vector<TeleportProbeCandidate> candidates =
        BuildTeleportProbeCandidates(*holder, aim, holder_idx, entity_idx, state, graphics);
    const TeleportProbeCandidate* const chosen = FindFirstValidTeleportProbeCandidate(candidates);
    const TeleportProbeCandidate* const blocked = FindFirstBlockedTeleportProbeCandidate(candidates);

    (void)PlayEntityCenterSoundEmitter(state, *holder, audio_asset_ids::Teleport);
    const Vec2 source_center =
        entities::common::GetVisualCenterForEntity(*holder, graphics, holder->GetCenter());
    AddEntityShake(*holder, 0.5F);
    AddEntityShake(teleporter, 0.5F);
    ApplyTeleportAreaShake(state, source_center, 8.0F, 8.0F, 1.0F, 1.5F);
    SpawnTelefragSplitEffect(*holder, graphics, aim.direction, state);
    SpawnTelefragSplitEffect(teleporter, graphics, aim.direction, state);

    if (chosen == nullptr) {
        if (blocked != nullptr) {
            MoveTeleportHolderToDestination(*holder, holder_idx, blocked->destination_center, state, graphics);
            AddEntityShake(*holder, 0.5F);
            AddEntityShake(teleporter, 0.5F);
            ApplyTeleportAreaShake(state, blocked->destination_center, 8.0F, 8.0F, 1.0F, 1.5F);
            SpawnTelefragMergeEffect(*holder, graphics, aim.direction, state);
            SpawnTelefragMergeEffect(teleporter, graphics, aim.direction, state);
            CrushTeleportHolder(holder_idx, state, audio);
            SplatTelefraggedEntity(entity_idx, state, graphics);
        } else {
            SplatTelefraggedEntity(entity_idx, state, graphics);
            KillTeleportHolder(holder_idx, state, audio);
        }
        return;
    }

    ApplyTelefragToCandidate(*chosen, state, audio, graphics);
    MoveTeleportHolderToDestination(*holder, holder_idx, chosen->destination_center, state, graphics);
    AddEntityShake(*holder, 0.5F);
    AddEntityShake(teleporter, 0.5F);
    ApplyTeleportAreaShake(state, chosen->destination_center, 8.0F, 8.0F, 1.0F, 1.5F);
    SpawnTelefragMergeEffect(*holder, graphics, aim.direction, state);
    SpawnTelefragMergeEffect(teleporter, graphics, aim.direction, state);
}

void StepEntityLogicAsTeleporter(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }
    Entity& teleporter = state.entity_manager.entities[entity_idx];
    if (!teleporter.active) {
        return;
    }

    const Entity* holder = nullptr;
    if (teleporter.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntity(*teleporter.held_by_vid);
    }

    if (teleporter.type_ == EntityType::TeleporterBackpack) {
        SetAnimation(teleporter, GetTeleporterBackpackAnimation(teleporter, holder, graphics));
    } else {
        SetAnimation(teleporter, frame_data_ids::Teleporter);
    }

    if (!teleporter.held_by_vid.has_value() || holder == nullptr || !holder->active) {
        return;
    }

    AddTeleporterDebugAnnotations(teleporter, holder, holder->vid.id, entity_idx, state, graphics);
}

extern const EntityArchetype kTeleporterArchetype{
    .type_ = EntityType::Teleporter,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsTeleporter,
    .step_logic = StepEntityLogicAsTeleporter,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Teleporter),
};

extern const EntityArchetype kTeleporterBackpackArchetype{
    .type_ = EntityType::TeleporterBackpack,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsTeleporter,
    .step_logic = StepEntityLogicAsTeleporter,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::TeleporterBackpack),
};

} // namespace splonks::entities::teleporter
