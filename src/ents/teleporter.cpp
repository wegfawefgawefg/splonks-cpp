#include "ents/teleporter.hpp"

#include "audio_asset_id.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "particles/sprite_particle.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <optional>
#include <string>
#include <vector>

namespace splonks::ents::teleporter {

namespace {

constexpr int kTeleportCardinalMinTiles = 4;
constexpr int kTeleportCardinalMaxTiles = 8;
constexpr int kTeleportDiagonalMinTiles = 3;
constexpr int kTeleportDiagonalMaxTiles = 6;
constexpr std::uint32_t kTelefragDamage = 9999;
constexpr float kDiagonalAxisComponent = 0.707106769F;

enum class TeleportProbeBlockReason {
    None,
    World,
    BlockingEnt,
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
    sim::AABB destination_aabb = sim::AABB::from_corners(sim::Vec2::zero(), sim::Vec2::zero());
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
    const int x = std::clamp(direction.x, -1, 1);
    const int y = std::clamp(direction.y, -1, 1);
    if (x == 0 && y == 0) {
        return Vec2::New(1.0F, 0.0F);
    }
    if (x != 0 && y != 0) {
        return Vec2::New(
            static_cast<float>(x) * kDiagonalAxisComponent,
            static_cast<float>(y) * kDiagonalAxisComponent
        );
    }
    return Vec2::New(static_cast<float>(x), static_cast<float>(y));
}

Vec2 GetTeleportOrtho(const Vec2& axis) {
    return Vec2::New(-axis.y, axis.x);
}

bool HasTeleportAnim(const Graphics& graphics, AFrameId anim_id) {
    return graphics.aframe_db.FindFrame(anim_id, 0) != nullptr;
}

AFrameId GetTeleporterBackpackAnim(const Ent& teleporter, const Ent* holder, const Graphics& graphics) {
    if (teleporter.attach_mode == AttachMode::Back) {
        if (holder != nullptr) {
            if (holder->IsHanging() && HasTeleportAnim(graphics, aframe_ids::TeleporterBackpackSide)) {
                return aframe_ids::TeleporterBackpackSide;
            }
            if (holder->IsClimbing() && HasTeleportAnim(graphics, aframe_ids::TeleporterBackpackBack)) {
                return aframe_ids::TeleporterBackpackBack;
            }
        }
        if (HasTeleportAnim(graphics, aframe_ids::TeleporterBackpackBack)) {
            return aframe_ids::TeleporterBackpackBack;
        }
    }
    if (teleporter.held_by_vid.has_value() && HasTeleportAnim(graphics, aframe_ids::TeleporterBackpackSide)) {
        return aframe_ids::TeleporterBackpackSide;
    }
    if (HasTeleportAnim(graphics, aframe_ids::TeleporterBackpack)) {
        return aframe_ids::TeleporterBackpack;
    }
    return aframe_ids::Teleporter;
}

TeleportAim GetTeleportAim(const Ent& teleporter, const Ent& holder, const State& state) {
    const controls::ControlIntent intent = controls::GetControlIntentForEnt(holder, state);

    const int dx = (intent.right ? 1 : 0) - (intent.left ? 1 : 0);
    const int dy = (intent.down ? 1 : 0) - (intent.up ? 1 : 0);
    IVec2 direction = IVec2::New(dx, dy);
    if (direction.x == 0 && direction.y == 0) {
        direction = teleporter.facing == Side::Left ? IVec2::New(-1, 0) : IVec2::New(1, 0);
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

Ent BuildTeleporterProbeEnt(
    const Ent& holder,
    const Graphics& graphics,
    const Vec2& destination_center
) {
    Ent probe = holder;
    common::SetVisualCenterForEnt(probe, graphics, destination_center);
    return probe;
}

bool DoesEntBlockTeleportDestination(const Ent& ent) {
    if (!ent.active || ent.condition == EntCondition::Dead) {
        return false;
    }
    if (ent.held_by_vid.has_value() || ent.attach_mode != AttachMode::None) {
        return true;
    }
    if (!ent.can_collide) {
        return false;
    }
    if (ent.impassable) {
        return true;
    }
    return !ent.can_be_hit;
}

bool IsAttackKillableActor(const Ent& ent) {
    return (ent.can_be_stunned || ent.hurt_on_contact || ent.alignment == Alignment::Enemy) &&
           common::CanEntTakeDamageType(ent, DamageType::Attack);
}

bool IsAttackKillableBreakable(const Ent& ent) {
    return ent.on_death != nullptr && common::CanEntTakeDamageType(ent, DamageType::Attack);
}

bool IsAttackReactiveProp(const Ent& ent) {
    return ent.on_damage != nullptr;
}

bool CanTelefragEnt(const Ent& ent) {
    if (!ent.active || !ent.can_collide || ent.condition == EntCondition::Dead) {
        return false;
    }
    if (ent.held_by_vid.has_value() || ent.attach_mode != AttachMode::None) {
        return false;
    }
    if (ent.impassable || !ent.can_be_hit) {
        return false;
    }

    return IsAttackKillableActor(ent) ||
           IsAttackKillableBreakable(ent) ||
           IsAttackReactiveProp(ent);
}

bool CanSplatDeadEnt(const Ent& ent) {
    if (!ent.active || ent.condition != EntCondition::Dead) {
        return false;
    }
    if (ent.held_by_vid.has_value() || ent.attach_mode != AttachMode::None) {
        return false;
    }
    if (!ent.can_collide || ent.impassable) {
        return false;
    }
    return true;
}

bool DoesProbeOverlapEnt(
    sim::AABB probe_aabb,
    const Ent& other,
    const State& state,
    const Graphics& graphics
) {
    const sim::AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        probe_aabb.center(),
        common::GetContactAabbForEnt(other, graphics)
    );
    return gfxp::aabbs_intersect(probe_aabb, other_aabb);
}

TeleportProbeCandidate EvaluateTeleportProbeCandidate(
    const Ent& holder,
    const TeleportAim& aim,
    std::size_t holder_idx,
    std::size_t teleporter_idx,
    int distance_tiles,
    State& state,
    const Graphics& graphics
) {
    const Vec2 holder_visual_center = common::GetVisualCenterForEnt(holder, graphics, holder.GetCenter());
    const IVec2 holder_tile = state.stage.GetTileCoordAtWc(ToIVec2(holder_visual_center));
    const IVec2 raw_target_tile = holder_tile + IVec2::New(aim.direction.x * distance_tiles, aim.direction.y * distance_tiles);
    const IVec2 target_tile = state.stage.WrapTileCoord(raw_target_tile);
    const Vec2 destination_center = TileCenterForTilePos(target_tile);

    TeleportProbeCandidate candidate{
        .distance_tiles = distance_tiles,
        .tile_pos = target_tile,
        .destination_center = destination_center,
        .destination_aabb = sim::AABB::from_corners(sim::Vec2::zero(), sim::Vec2::zero()),
        .block_reason = TeleportProbeBlockReason::None,
        .telefrag_vids = {},
        .splat_vids = {},
    };

    const Ent probe = BuildTeleporterProbeEnt(holder, graphics, destination_center);
    candidate.destination_aabb = common::GetContactAabbForEnt(probe, graphics);

    if (AabbHitsBlockingWorldGeometryOrImpassableEnts(
            state,
            graphics,
            candidate.destination_aabb,
            holder.vid
        )) {
        candidate.block_reason = TeleportProbeBlockReason::World;
        return candidate;
    }

    for (const VID& other_vid : QueryEntsInAabb(state, candidate.destination_aabb, holder.vid)) {
        if (other_vid.id == teleporter_idx || other_vid.id == holder_idx) {
            continue;
        }

        const Ent* const other = state.ents.GetEnt(other_vid);
        if (other == nullptr || !DoesProbeOverlapEnt(candidate.destination_aabb, *other, state, graphics)) {
            continue;
        }
        if (CanSplatDeadEnt(*other)) {
            candidate.splat_vids.push_back(other_vid);
            continue;
        }
        if (DoesEntBlockTeleportDestination(*other)) {
            candidate.block_reason = TeleportProbeBlockReason::BlockingEnt;
            candidate.telefrag_vids.clear();
            candidate.splat_vids.clear();
            return candidate;
        }
        if (CanTelefragEnt(*other)) {
            candidate.telefrag_vids.push_back(other_vid);
        }
    }

    return candidate;
}

std::vector<TeleportProbeCandidate> BuildTeleportProbeCandidates(
    const Ent& holder,
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
    const Ent& ent,
    const Graphics& graphics,
    const Vec2& visual_center,
    const Vec2& start_offset,
    const Vec2& velocity,
    float tint_r,
    float tint_g,
    float tint_b,
    State& state
) {
    const AFrame* const aframe = common::GetCurrentAFrameForEnt(ent, graphics);
    if (aframe == nullptr) {
        return;
    }

    SpriteParticle particle{};
    particle.counter = 32;
    particle.draw_layer = ent.draw_layer;
    particle.lighting_mode = ParticleLightingMode::Emissive;
    particle.pos = visual_center + start_offset;
    particle.size = Vec2::New(
        static_cast<float>(aframe->sample_rect.w),
        static_cast<float>(aframe->sample_rect.h)
    ) * sim::ToRenderScalar(ent.aframe_animator.scale);
    particle.rot = sim::ToRenderScalar(ent.rotation);
    particle.alpha = 0.85F;
    particle.tint_r = tint_r;
    particle.tint_g = tint_g;
    particle.tint_b = tint_b;
    particle.horizontal_flip = ent.facing == Side::Right;
    particle.vel = velocity;
    particle.alpha_vel = -0.0275F;
    particle.aframe_animator = ent.aframe_animator;
    particle.aframe_animator.animate = false;
    state.particles.Add(std::move(particle));
}

void SpawnTelefragSplitEffectAt(
    const Ent& ent,
    const Graphics& graphics,
    const Vec2& visual_center,
    const IVec2& direction,
    State& state
) {
    const Vec2 axis = GetTeleportAxis(direction);
    const Vec2 ortho = GetTeleportOrtho(axis);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, Vec2::New(0.0F, 0.0F), (axis * -0.3F) - (ortho * 0.0625F), 1.0F, 0.20F, 0.20F, state);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, Vec2::New(0.0F, 0.0F), ortho * 0.0375F, 0.25F, 1.0F, 0.25F, state);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, Vec2::New(0.0F, 0.0F), (axis * 0.3F) - (ortho * 0.0625F), 0.30F, 0.30F, 1.0F, state);
}

void SpawnTelefragSplitEffect(const Ent& ent, const Graphics& graphics, const IVec2& direction, State& state) {
    SpawnTelefragSplitEffectAt(
        ent,
        graphics,
        common::GetVisualCenterForEnt(ent, graphics, ent.GetCenter()),
        direction,
        state
    );
}

void SpawnTelefragMergeEffectAt(
    const Ent& ent,
    const Graphics& graphics,
    const Vec2& visual_center,
    const IVec2& direction,
    State& state
) {
    const Vec2 axis = GetTeleportAxis(direction);
    const Vec2 ortho = GetTeleportOrtho(axis);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, axis * -3.0F, axis * 0.3F, 1.0F, 0.20F, 0.20F, state);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, ortho * 2.0F, ortho * -0.0875F, 0.25F, 1.0F, 0.25F, state);
    SpawnTelefragPhaseParticle(ent, graphics, visual_center, axis * 3.0F, axis * -0.3F, 0.30F, 0.30F, 1.0F, state);
}

void SpawnTelefragMergeEffect(const Ent& ent, const Graphics& graphics, const IVec2& direction, State& state) {
    SpawnTelefragMergeEffectAt(
        ent,
        graphics,
        common::GetVisualCenterForEnt(ent, graphics, ent.GetCenter()),
        direction,
        state
    );
}

void SplatTelefraggedEnt(std::size_t ent_idx, State& state, const Graphics& graphics) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    if (!ent.active || ent.impassable) {
        return;
    }

    SpawnTelefragSplitEffect(ent, graphics, IVec2::New(1, 0), state);
    common::DropHeldItemFromEnt(ent, state);
    common::ReleaseEntFromHolder(ent, state);
    ent.marked_for_destruction = true;
    (void)world_ops::DeactivateEnt(state, ent.vid);
}

void ApplyTelefragToCandidate(const TeleportProbeCandidate& candidate, State& state, Audio& audio, const Graphics& graphics) {
    for (const VID& other_vid : candidate.splat_vids) {
        SplatTelefraggedEnt(other_vid.id, state, graphics);
    }

    for (const VID& other_vid : candidate.telefrag_vids) {
        if (other_vid.id >= state.ents.ents.size()) {
            continue;
        }

        const common::DamageResult damage_result =
            common::TryDamageEnt(other_vid.id, state, audio, DamageType::Attack, kTelefragDamage);
        if (damage_result == common::DamageResult::Died) {
            SplatTelefraggedEnt(other_vid.id, state, graphics);
        }
    }
}

void MoveTeleportHolderToDestination(
    Ent& holder,
    std::size_t holder_idx,
    const Vec2& destination_center,
    State& state,
    const Graphics& graphics
) {
    common::SetVisualCenterForEnt(holder, graphics, destination_center);
    holder.pos = sim::PixelVec2(holder.pos.x.to_pixels_round(), holder.pos.y.to_pixels_round());
    holder.grounded = false;
    holder.hang_side.reset();
    SetMovementFlag(holder, EntMovementFlag::Climbing, false);
    SetMovementFlag(holder, EntMovementFlag::Hanging, false);
    holder.climb_detach_cooldown = 0;
    state.UpdateSidForEnt(holder_idx, graphics);
    common::SyncEntAttachs(holder_idx, state, graphics);
}

void ApplyTeleportAreaShake(
    State& state,
    const Vec2& world_pos,
    float foreground_tile_amount,
    float background_tile_amount,
    float ent_amount,
    float radius_tiles
) {
    AddShake(
        state,
        world_pos,
        foreground_tile_amount,
        background_tile_amount,
        ent_amount,
        radius_tiles,
        std::nullopt
    );
}

void AddTeleporterDebugAnnotations(
    const Ent& teleporter,
    const Ent* holder,
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
        } else if (candidate.block_reason == TeleportProbeBlockReason::BlockingEnt) {
            color = DebugAnnotationColor{255, 0, 255, 255};
        }

        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = ToRenderAABB(candidate.destination_aabb),
            .color = color,
        });

        std::string text = "tp " + std::to_string(candidate.distance_tiles) + " (" +
                           std::to_string(candidate.tile_pos.x) + ", " +
                           std::to_string(candidate.tile_pos.y) + ")";
        if (candidate.block_reason == TeleportProbeBlockReason::World) {
            text += " blocked";
        } else if (candidate.block_reason == TeleportProbeBlockReason::BlockingEnt) {
            text += " ent";
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
    if (holder_idx >= state.ents.ents.size()) {
        return;
    }
    Ent& holder = state.ents.ents[holder_idx];
    holder.health = 0;
    common::DieIfDead(holder_idx, state, audio);
}

void CrushTeleportHolder(std::size_t holder_idx, State& state, Audio& audio) {
    if (holder_idx >= state.ents.ents.size()) {
        return;
    }
    (void)common::TryDamageEnt(holder_idx, state, audio, DamageType::Crush, 1);
}

} // namespace

void OnUseAsTeleporter(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    Ent& teleporter = state.ents.ents[ent_idx];
    if (!teleporter.use_state.pressed || !teleporter.held_by_vid.has_value()) {
        return;
    }

    Ent* const holder = state.ents.GetEntMut(*teleporter.held_by_vid);
    if (holder == nullptr || !holder->active) {
        return;
    }

    const std::size_t holder_idx = holder->vid.id;
    const TeleportAim aim = GetTeleportAim(teleporter, *holder, state);
    const std::vector<TeleportProbeCandidate> candidates =
        BuildTeleportProbeCandidates(*holder, aim, holder_idx, ent_idx, state, graphics);
    const TeleportProbeCandidate* const chosen = FindFirstValidTeleportProbeCandidate(candidates);
    const TeleportProbeCandidate* const blocked = FindFirstBlockedTeleportProbeCandidate(candidates);

    (void)PlayEntCenterSoundEmitter(state, *holder, audio_asset_ids::Teleport);
    const Vec2 source_center =
        ents::common::GetVisualCenterForEnt(*holder, graphics, holder->GetCenter());
    AddEntShake(*holder, 0.5F);
    AddEntShake(teleporter, 0.5F);
    ApplyTeleportAreaShake(state, source_center, 8.0F, 8.0F, 1.0F, 1.5F);
    SpawnTelefragSplitEffect(*holder, graphics, aim.direction, state);
    SpawnTelefragSplitEffect(teleporter, graphics, aim.direction, state);

    if (chosen == nullptr) {
        if (blocked != nullptr) {
            MoveTeleportHolderToDestination(*holder, holder_idx, blocked->destination_center, state, graphics);
            AddEntShake(*holder, 0.5F);
            AddEntShake(teleporter, 0.5F);
            ApplyTeleportAreaShake(state, blocked->destination_center, 8.0F, 8.0F, 1.0F, 1.5F);
            SpawnTelefragMergeEffect(*holder, graphics, aim.direction, state);
            SpawnTelefragMergeEffect(teleporter, graphics, aim.direction, state);
            CrushTeleportHolder(holder_idx, state, audio);
            SplatTelefraggedEnt(ent_idx, state, graphics);
        } else {
            SplatTelefraggedEnt(ent_idx, state, graphics);
            KillTeleportHolder(holder_idx, state, audio);
        }
        return;
    }

    ApplyTelefragToCandidate(*chosen, state, audio, graphics);
    MoveTeleportHolderToDestination(*holder, holder_idx, chosen->destination_center, state, graphics);
    AddEntShake(*holder, 0.5F);
    AddEntShake(teleporter, 0.5F);
    ApplyTeleportAreaShake(state, chosen->destination_center, 8.0F, 8.0F, 1.0F, 1.5F);
    SpawnTelefragMergeEffect(*holder, graphics, aim.direction, state);
    SpawnTelefragMergeEffect(teleporter, graphics, aim.direction, state);
}

void StepEntLogicAsTeleporter(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }
    Ent& teleporter = state.ents.ents[ent_idx];
    if (!teleporter.active) {
        return;
    }

    const Ent* holder = nullptr;
    if (teleporter.held_by_vid.has_value()) {
        holder = state.ents.GetEnt(*teleporter.held_by_vid);
    }

    if (teleporter.type_ == EntType::TeleporterBackpack) {
        SetAnim(teleporter, GetTeleporterBackpackAnim(teleporter, holder, graphics));
    } else {
        SetAnim(teleporter, aframe_ids::Teleporter);
    }

    if (!teleporter.held_by_vid.has_value() || holder == nullptr || !holder->active) {
        return;
    }

    AddTeleporterDebugAnnotations(teleporter, holder, holder->vid.id, ent_idx, state, graphics);
}

extern const EntSpec kTeleporterSpec{
    .type_ = EntType::Teleporter,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsTeleporter,
    .step_logic = StepEntLogicAsTeleporter,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Teleporter),
};

extern const EntSpec kTeleporterBackpackSpec{
    .type_ = EntType::TeleporterBackpack,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .on_use = OnUseAsTeleporter,
    .step_logic = StepEntLogicAsTeleporter,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::TeleporterBackpack),
};

} // namespace splonks::ents::teleporter
