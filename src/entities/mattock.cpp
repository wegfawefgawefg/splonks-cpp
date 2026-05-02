#include "entities/mattock.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "entities/common/common.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "stage.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "tile_archetype.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>
#include <string>

namespace splonks::entities::mattock {

namespace {

constexpr float kMattockStrikePending = 1.0F;
constexpr float kMattockGuaranteedDigs = 10.0F;
constexpr int kMattockBreakChancePercentAfterGuaranteedDigs = 10;
constexpr int kMattockForwardProbeBiasPixels = 6;
constexpr int kMattockVerticalProbeOffsetPixels = 7;

enum class StrikeResult {
    Miss,
    HitUnbreakable,
    Dug,
};

struct StrikeOutcome {
    StrikeResult result = StrikeResult::Miss;
    Vec2 sound_pos = Vec2::New(0.0F, 0.0F);
};

struct EntityStrikeOutcome {
    bool hit_any = false;
    Vec2 sound_pos = Vec2::New(0.0F, 0.0F);
};

struct MattockTileTargets {
    IVec2 primary = IVec2::New(0, 0);
    IVec2 secondary = IVec2::New(0, 0);
    Vec2 primary_probe_world = Vec2::New(0.0F, 0.0F);
    Vec2 secondary_probe_world = Vec2::New(0.0F, 0.0F);
};

bool IsSwinging(const Entity& mattock) {
    return mattock.frame_data_animator.animation_id == frame_data_ids::MattockSwing;
}

AABB TileAabbForTilePos(const IVec2& tile_pos) {
    const Vec2 tile_tl = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
    );
    return AABB::New(
        tile_tl,
        tile_tl + Vec2::New(
                      static_cast<float>(kTileSize - 1),
                      static_cast<float>(kTileSize - 1)
                  )
    );
}

Vec2 GetFallbackStrikePoint(const Entity& mattock) {
    const float direction = mattock.facing == LeftOrRight::Left ? -1.0F : 1.0F;
    return mattock.GetCenter() + Vec2::New(10.0F * direction, 0.0F);
}

void SpawnMattockImpactParticles(State& state, const Vec2& pos, int direction) {
    for (int i = 0; i < 3; ++i) {
        SpriteParticle spark{};
        spark.frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spark);
        spark.draw_layer = DrawLayer::Foreground;
        spark.lighting_mode = ParticleLightingMode::Emissive;
        spark.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        spark.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 6.0F));
        spark.rot = rng::RandomFloat(0.0F, 360.0F);
        spark.alpha = 1.0F;
        spark.vel = Vec2::New(
            rng::RandomFloat(0.08F, 0.35F) * static_cast<float>(direction),
            rng::RandomFloat(-0.18F, 0.18F)
        );
        spark.svel = Vec2::New(-0.12F, -0.12F);
        spark.rotvel = rng::RandomFloat(-6.0F, 6.0F);
        spark.alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }

    for (int i = 0; i < 2; ++i) {
        SpriteParticle smoke{};
        smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(10, 16));
        smoke.pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        smoke.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.75F, 0.95F);
        smoke.vel = Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.18F, -0.06F));
        smoke.svel = Vec2::New(rng::RandomFloat(0.06F, 0.14F), rng::RandomFloat(0.06F, 0.14F));
        smoke.rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke.alpha_vel = -0.05F;
        smoke.acc = Vec2::New(0.0F, -0.01F);
        smoke.sacc = Vec2::New(0.01F, 0.01F);
        smoke.rotacc = 0.0F;
        smoke.alpha_acc = -0.003F;
        state.particles.Add(std::move(smoke));
    }
}

StrikeOutcome TryStrikeTileCoord(const IVec2& tile_pos, State& state, Audio& audio) {
    const Vec2 sound_pos = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );

    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtTilePos(state.stage, tile_pos);
    if (tile_query.has_value() && tile_query->tile != nullptr) {
        if (!GetTileArchetype(*tile_query->tile).solid) {
            return StrikeOutcome{
                .result = StrikeResult::Miss,
                .sound_pos = sound_pos,
            };
        }

        BreakStageTilesInRectWc(
            TileAabbForTilePos(tile_query->tile_pos),
            state,
            audio,
            audio_asset_ids::SuccessfulDig,
            true
        );
        return StrikeOutcome{
            .result = StrikeResult::Dug,
            .sound_pos = sound_pos,
        };
    }

    const std::optional<StageBorderSideKind> border_side =
        state.stage.GetOutOfBoundsSideForTileCoord(tile_pos.x, tile_pos.y);
    if (border_side.has_value() && state.stage.IsBorderSideBlocking(*border_side)) {
        return StrikeOutcome{
            .result = StrikeResult::HitUnbreakable,
            .sound_pos = sound_pos,
        };
    }

    return StrikeOutcome{
        .result = StrikeResult::Miss,
        .sound_pos = sound_pos,
    };
}

MattockTileTargets GetMattockTileTargets(const Entity& holder, const Stage& stage) {
    const auto [holder_tl, holder_br] = holder.GetBounds();
    const int front_world_x = holder.facing == LeftOrRight::Left
                                  ? static_cast<int>(std::floor(holder_tl.x)) - 1 -
                                        kMattockForwardProbeBiasPixels
                                  : static_cast<int>(std::floor(holder_br.x)) + 1 +
                                        kMattockForwardProbeBiasPixels;
    const int strike_world_y = static_cast<int>(std::floor(holder_br.y)) -
                               kMattockVerticalProbeOffsetPixels;
    const Vec2 primary_probe_world = Vec2::New(
        static_cast<float>(front_world_x),
        static_cast<float>(strike_world_y)
    );
    const Vec2 secondary_probe_world = Vec2::New(
        static_cast<float>(front_world_x),
        static_cast<float>(strike_world_y + static_cast<int>(kTileSize))
    );
    return MattockTileTargets{
        .primary = stage.GetTileCoordAtWc(ToIVec2(primary_probe_world)),
        .secondary = stage.GetTileCoordAtWc(ToIVec2(secondary_probe_world)),
        .primary_probe_world = primary_probe_world,
        .secondary_probe_world = secondary_probe_world,
    };
}

void AddMattockDebugAnnotations(
    const Entity& mattock,
    const Entity* holder,
    State& state,
    const Graphics& graphics
) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = common::GetContactAabbForEntity(mattock, graphics),
        .color = DebugAnnotationColor{0, 255, 255, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = mattock.GetCenter(),
        .text = "mattock cbox",
        .color = DebugAnnotationColor{0, 255, 255, 255},
    });

    if (holder == nullptr) {
        const Vec2 fallback = GetFallbackStrikePoint(mattock);
        const IVec2 fallback_tile = state.stage.GetTileCoordAtWc(ToIVec2(fallback));
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = TileAabbForTilePos(fallback_tile),
            .color = DebugAnnotationColor{255, 0, 0, 255},
        });
        state.AddDebugLabelAnnotation(DebugLabelAnnotation{
            .world_pos = fallback,
            .text = "fallback (" + std::to_string(fallback_tile.x) + ", " + std::to_string(fallback_tile.y) + ")",
            .color = DebugAnnotationColor{255, 0, 0, 255},
        });
        return;
    }

    const MattockTileTargets targets = GetMattockTileTargets(*holder, state.stage);
    const AABB primary_aabb = TileAabbForTilePos(targets.primary);
    const AABB secondary_aabb = TileAabbForTilePos(targets.secondary);
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = primary_aabb,
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = (primary_aabb.tl + primary_aabb.br) * 0.5F,
        .text = "dig 1 (" + std::to_string(targets.primary.x) + ", " + std::to_string(targets.primary.y) + ")",
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = targets.primary_probe_world,
        .text = "probe 1",
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = secondary_aabb,
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = (secondary_aabb.tl + secondary_aabb.br) * 0.5F,
        .text = "dig 2 (" + std::to_string(targets.secondary.x) + ", " + std::to_string(targets.secondary.y) + ")",
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = targets.secondary_probe_world,
        .text = "probe 2",
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
}

bool ShouldBreakMattockAfterSuccessfulDig(Entity& mattock) {
    if (mattock.counter_b > 0.0F) {
        mattock.counter_b -= 1.0F;
        return false;
    }

    return rng::RandomIntInclusive(1, 100) <= kMattockBreakChancePercentAfterGuaranteedDigs;
}

bool CanMattockHitEntity(const Entity& mattock, const Entity* holder, const Entity& other_entity) {
    if (!other_entity.active || !other_entity.can_collide || other_entity.condition == EntityCondition::Dead) {
        return false;
    }
    if (other_entity.vid == mattock.vid) {
        return false;
    }
    if (holder != nullptr && other_entity.vid == holder->vid) {
        return false;
    }
    if (holder != nullptr && other_entity.held_by_vid.has_value() && *other_entity.held_by_vid == holder->vid) {
        return false;
    }
    return true;
}

DamageType GetMattockDamageType(const Entity& other_entity) {
    if (other_entity.impassable || other_entity.stone) {
        return DamageType::Explosion;
    }
    return DamageType::Attack;
}

EntityStrikeOutcome TryStrikeEntitiesWithMattock(
    const Entity& mattock,
    const Entity* holder,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const AABB strike_aabb = common::GetContactAabbForEntity(mattock, graphics);
    EntityStrikeOutcome result{};

    for (const VID& other_vid : QueryEntitiesInAabb(state, strike_aabb, mattock.vid)) {
        const Entity* const other_entity_const = state.entity_manager.GetEntity(other_vid);
        if (other_entity_const == nullptr || !CanMattockHitEntity(mattock, holder, *other_entity_const)) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            mattock.GetCenter(),
            common::GetContactAabbForEntity(*other_entity_const, graphics)
        );
        if (!AabbsIntersect(strike_aabb, other_aabb)) {
            continue;
        }

        const DamageType damage_type = GetMattockDamageType(*other_entity_const);
        const bool is_heavy_target = other_entity_const->impassable || other_entity_const->stone;
        const common::DamageResult damage_result =
            common::TryDamageEntity(other_entity_const->vid.id, state, audio, damage_type, 1);
        if (damage_result == common::DamageResult::None) {
            continue;
        }
        if (is_heavy_target && damage_result == common::DamageResult::Died) {
            (void)PlayWorldSoundEmitter(state, (other_aabb.tl + other_aabb.br) * 0.5F, audio_asset_ids::PotShatter);
        }

        if (Entity* const other_entity = state.entity_manager.GetEntityMut(other_entity_const->vid)) {
            if (!other_entity->impassable) {
                const float knockback_x = mattock.facing == LeftOrRight::Left ? -4.0F : 4.0F;
                common::ApplyKnockback(
                    *other_entity,
                    common::KnockbackSpec{
                        .velocity = Vec2::New(knockback_x, -2.0F),
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .thrown_by = holder != nullptr ? std::optional<VID>(holder->vid) : std::nullopt,
                        .thrown_immunity_timer = common::kThrownByImmunityDuration,
                        .projectile_contact_damage_type = damage_type,
                        .projectile_contact_damage_amount = 1,
                        .projectile_contact_duration = common::kProjectileContactDuration,
                    }
                );
            }
            result.sound_pos = other_entity->GetCenter();
        } else {
            result.sound_pos = (other_aabb.tl + other_aabb.br) * 0.5F;
        }
        result.hit_any = true;
    }

    return result;
}

StrikeOutcome ComputeMattockStrikeOutcome(
    const Entity& mattock,
    const Entity* holder,
    State& state,
    Audio& audio
) {
    if (holder == nullptr) {
        const Vec2 fallback = GetFallbackStrikePoint(mattock);
        const IVec2 tile_pos = state.stage.GetTileCoordAtWc(ToIVec2(fallback));
        return TryStrikeTileCoord(tile_pos, state, audio);
    }

    const MattockTileTargets targets = GetMattockTileTargets(*holder, state.stage);
    const IVec2 primary = targets.primary;
    const IVec2 secondary = targets.secondary;

    const StrikeOutcome first = TryStrikeTileCoord(primary, state, audio);
    if (first.result != StrikeResult::Miss) {
        return first;
    }
    return TryStrikeTileCoord(secondary, state, audio);
}

void TryApplyMattockStrike(std::size_t entity_idx, State& state, const Graphics& graphics, Audio& audio) {
    Entity& mattock = state.entity_manager.entities[entity_idx];
    Entity* holder = nullptr;
    if (mattock.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntityMut(*mattock.held_by_vid);
    }

    const EntityStrikeOutcome entity_outcome =
        TryStrikeEntitiesWithMattock(mattock, holder, state, graphics, audio);
    const StrikeOutcome tile_outcome = ComputeMattockStrikeOutcome(mattock, holder, state, audio);
    if (!entity_outcome.hit_any && tile_outcome.result == StrikeResult::Miss) {
        return;
    }

    const int direction = mattock.facing == LeftOrRight::Left ? -1 : 1;
    const Vec2 effect_pos = tile_outcome.result != StrikeResult::Miss ? tile_outcome.sound_pos
                                                                      : entity_outcome.sound_pos;
    SpawnMattockImpactParticles(state, effect_pos, direction);
    (void)PlayWorldSoundEmitter(state, effect_pos, audio_asset_ids::Pickaxe);
    if (tile_outcome.result == StrikeResult::HitUnbreakable) {
        (void)PlayWorldSoundEmitter(state, tile_outcome.sound_pos, audio_asset_ids::UnbreakableHit);
        return;
    }
    if (tile_outcome.result == StrikeResult::Dug && ShouldBreakMattockAfterSuccessfulDig(mattock)) {
        mattock.marked_for_destruction = true;
    }
}

} // namespace

void OnUseAsMattock(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)state;
    (void)graphics;
    (void)audio;
    Entity& mattock = state.entity_manager.entities[entity_idx];
    if (!mattock.use_state.pressed || IsSwinging(mattock)) {
        return;
    }

    SetAnimation(mattock, frame_data_ids::MattockSwing);
    mattock.frame_data_animator.loop = false;
    mattock.counter_a = kMattockStrikePending;

    if (mattock.use_state.source == AttachmentMode::None) {
        StopUsingEntity(mattock);
    }
}

void StepEntityLogicAsMattock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    Entity& mattock = state.entity_manager.entities[entity_idx];

    const Entity* holder = nullptr;
    if (mattock.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntity(*mattock.held_by_vid);
    }
    AddMattockDebugAnnotations(mattock, holder, state, graphics);

    if (!IsSwinging(mattock)) {
        return;
    }

    if (mattock.counter_a > 0.0F && mattock.frame_data_animator.current_frame > 0) {
        TryApplyMattockStrike(entity_idx, state, graphics, audio);
        mattock.counter_a = 0.0F;
    }

    if (!mattock.frame_data_animator.IsFinished()) {
        return;
    }

    SetAnimation(mattock, frame_data_ids::Mattock);
    mattock.frame_data_animator.loop = true;
}

extern const EntityArchetype kMattockArchetype{
    .type_ = EntityType::Mattock,
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
    .counter_b = kMattockGuaranteedDigs,
    .damage_vulnerability = DamageVulnerability::Immune,
    .on_use = OnUseAsMattock,
    .step_logic = StepEntityLogicAsMattock,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Mattock),
};

} // namespace splonks::entities::mattock
