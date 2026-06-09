#include "ents/mattock.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "ents/common/common.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "stage.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "tile_spec.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <memory>
#include <string>

namespace splonks::ents::mattock {

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
    FVec2 sound_pos = FVec2::New(0.0F, 0.0F);
};

struct EntStrikeOutcome {
    bool hit_any = false;
    FVec2 sound_pos = FVec2::New(0.0F, 0.0F);
};

struct MattockTileTargets {
    IVec2 primary = IVec2::New(0, 0);
    IVec2 secondary = IVec2::New(0, 0);
    FVec2 primary_probe_world = FVec2::New(0.0F, 0.0F);
    FVec2 secondary_probe_world = FVec2::New(0.0F, 0.0F);
};

bool IsSwinging(const Ent& mattock) {
    return mattock.aframe_animator.anim_id == aframe_ids::MattockSwing;
}

[[nodiscard]] FAABB RenderTileAabbForTilePos(const IVec2& tile_pos) {
    const FVec2 tile_tl = FVec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
    );
    return FAABB::New(
        tile_tl,
        tile_tl + FVec2::New(
                      static_cast<float>(kTileSize - 1),
                      static_cast<float>(kTileSize - 1)
                  )
    );
}

sim::FxAABB SimTileAabbForTilePos(const IVec2& tile_pos) {
    const sim::FxVec2 tile_tl = sim::PixelVec2(
        tile_pos.x * static_cast<int>(kTileSize),
        tile_pos.y * static_cast<int>(kTileSize)
    );
    return sim::FxAABB::from_corners(
        tile_tl,
        tile_tl + sim::PixelVec2(static_cast<int>(kTileSize - 1), static_cast<int>(kTileSize - 1))
    );
}

IVec2 ToWorldPixelTrunc(sim::FxVec2 point) {
    return IVec2::New(point.x.to_pixels_trunc(), point.y.to_pixels_trunc());
}

sim::FxVec2 GetFallbackStrikePoint(const Ent& mattock) {
    const sim::Scalar direction =
        sim::Scalar::from_int(mattock.facing == Side::Left ? -1 : 1);
    return mattock.GetSimCenter() + sim::FxVec2{sim::Scalar::from_int(10) * direction,
                                              sim::Scalar::zero()};
}

void SpawnMattockImpactParticles(State& state, const FVec2& pos, int direction) {
    for (int i = 0; i < 3; ++i) {
        SpriteParticle spark{};
        spark.aframe_animator = AFrameAnimator::New(aframe_ids::Spark);
        spark.draw_layer = DrawLayer::Foreground;
        spark.lighting_mode = ParticleLightingMode::Emissive;
        spark.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark.pos = pos + FVec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        spark.size = FVec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 6.0F));
        spark.rot = rng::RandomFloat(0.0F, 360.0F);
        spark.alpha = 1.0F;
        spark.vel = FVec2::New(
            rng::RandomFloat(0.08F, 0.35F) * static_cast<float>(direction),
            rng::RandomFloat(-0.18F, 0.18F)
        );
        spark.svel = FVec2::New(-0.12F, -0.12F);
        spark.rotvel = rng::RandomFloat(-6.0F, 6.0F);
        spark.alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }

    for (int i = 0; i < 2; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(10, 16));
        smoke.pos = pos + FVec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        smoke.size = FVec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.75F, 0.95F);
        smoke.vel = FVec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.18F, -0.06F));
        smoke.svel = FVec2::New(rng::RandomFloat(0.06F, 0.14F), rng::RandomFloat(0.06F, 0.14F));
        smoke.rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke.alpha_vel = -0.05F;
        smoke.acc = FVec2::New(0.0F, -0.01F);
        smoke.sacc = FVec2::New(0.01F, 0.01F);
        smoke.rotacc = 0.0F;
        smoke.alpha_acc = -0.003F;
        state.particles.Add(std::move(smoke));
    }
}

StrikeOutcome TryStrikeTileCoord(const IVec2& tile_pos, State& state, Audio& audio) {
    const FVec2 sound_pos = FVec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );

    const std::optional<WorldTileQueryResult> tile_query = QueryTileAtTilePos(state.stage, tile_pos);
    if (tile_query.has_value() && tile_query->tile != nullptr) {
        if (!GetTileSpec(*tile_query->tile).solid) {
            return StrikeOutcome{
                .result = StrikeResult::Miss,
                .sound_pos = sound_pos,
            };
        }

        BreakStageTilesInRectWc(
            SimTileAabbForTilePos(tile_query->tile_pos),
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

MattockTileTargets GetMattockTileTargets(const Ent& holder, const Stage& stage) {
    const sim::FxAABB holder_aabb = holder.GetSimAABB();
    const int front_world_x = holder.facing == Side::Left
                                  ? holder_aabb.tl.x.to_pixels_floor() - 1 -
                                        kMattockForwardProbeBiasPixels
                                  : holder_aabb.br.x.to_pixels_floor() + 1 +
                                        kMattockForwardProbeBiasPixels;
    const int strike_world_y = holder_aabb.br.y.to_pixels_floor() -
                               kMattockVerticalProbeOffsetPixels;
    const FVec2 primary_probe_world = FVec2::New(
        static_cast<float>(front_world_x),
        static_cast<float>(strike_world_y)
    );
    const FVec2 secondary_probe_world = FVec2::New(
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
    const Ent& mattock,
    const Ent* holder,
    State& state,
    const Graphics& graphics
) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = ToFAABB(common::GetContactAabbForEnt(mattock, graphics)),
        .color = DebugAnnotationColor{0, 255, 255, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = mattock.GetRenderCenter(),
        .text = "mattock cbox",
        .color = DebugAnnotationColor{0, 255, 255, 255},
    });

    if (holder == nullptr) {
        const sim::FxVec2 fallback = GetFallbackStrikePoint(mattock);
        const IVec2 fallback_tile = state.stage.GetTileCoordAtWc(ToWorldPixelTrunc(fallback));
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = RenderTileAabbForTilePos(fallback_tile),
            .color = DebugAnnotationColor{255, 0, 0, 255},
        });
        state.AddDebugLabelAnnotation(DebugLabelAnnotation{
            .world_pos = sim::ToRenderVec2(fallback),
            .text = "fallback (" + std::to_string(fallback_tile.x) + ", " + std::to_string(fallback_tile.y) + ")",
            .color = DebugAnnotationColor{255, 0, 0, 255},
        });
        return;
    }

    const MattockTileTargets targets = GetMattockTileTargets(*holder, state.stage);
    const FAABB primary_render_aabb = RenderTileAabbForTilePos(targets.primary);
    const FAABB secondary_render_aabb = RenderTileAabbForTilePos(targets.secondary);
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = primary_render_aabb,
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = (primary_render_aabb.tl + primary_render_aabb.br) * 0.5F,
        .text = "dig 1 (" + std::to_string(targets.primary.x) + ", " + std::to_string(targets.primary.y) + ")",
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = targets.primary_probe_world,
        .text = "probe 1",
        .color = DebugAnnotationColor{0, 255, 0, 255},
    });
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = secondary_render_aabb,
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = (secondary_render_aabb.tl + secondary_render_aabb.br) * 0.5F,
        .text = "dig 2 (" + std::to_string(targets.secondary.x) + ", " + std::to_string(targets.secondary.y) + ")",
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = targets.secondary_probe_world,
        .text = "probe 2",
        .color = DebugAnnotationColor{255, 255, 0, 255},
    });
}

bool ShouldBreakMattockAfterSuccessfulDig(Ent& mattock, State& state) {
    if (mattock.counter_b > sim::Scalar::zero()) {
        mattock.counter_b -= sim::Scalar::from_int(1);
        return false;
    }

    return state.drng.RandomIntInclusive(1, 100) <=
           kMattockBreakChancePercentAfterGuaranteedDigs;
}

bool CanMattockHitEnt(const Ent& mattock, const Ent* holder, const Ent& other_ent) {
    if (!other_ent.active || !other_ent.can_collide || other_ent.condition == EntCondition::Dead) {
        return false;
    }
    if (other_ent.vid == mattock.vid) {
        return false;
    }
    if (holder != nullptr && other_ent.vid == holder->vid) {
        return false;
    }
    if (holder != nullptr && other_ent.held_by_vid.has_value() && *other_ent.held_by_vid == holder->vid) {
        return false;
    }
    return true;
}

DamageType GetMattockDamageType(const Ent& other_ent) {
    if (other_ent.impassable || other_ent.stone) {
        return DamageType::Explosion;
    }
    return DamageType::Attack;
}

EntStrikeOutcome TryStrikeEntsWithMattock(
    const Ent& mattock,
    const Ent* holder,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const sim::FxAABB strike_aabb = common::GetContactAabbForEnt(mattock, graphics);
    EntStrikeOutcome result{};

    for (const VID& other_vid : QueryEntsInAabb(state, strike_aabb, mattock.vid)) {
        const Ent* const other_ent_const = state.ents.GetEnt(other_vid);
        if (other_ent_const == nullptr || !CanMattockHitEnt(mattock, holder, *other_ent_const)) {
            continue;
        }

        const sim::FxAABB other_aabb = GetNearestWorldAabb(
            state.stage,
            strike_aabb.center(),
            common::GetContactAabbForEnt(*other_ent_const, graphics)
        );
        if (!gfxp::aabbs_intersect(strike_aabb, other_aabb)) {
            continue;
        }

        const DamageType damage_type = GetMattockDamageType(*other_ent_const);
        const bool is_heavy_target = other_ent_const->impassable || other_ent_const->stone;
        const sim::Scalar knockback_x =
            sim::Scalar::from_int(mattock.facing == Side::Left ? -4 : 4);
        const common::DamageResult damage_result =
            common::TryHitEnt(
                other_ent_const->vid.id,
                state,
                audio,
                damage_type,
                1,
                common::HitOptions{
                    .source_vid = mattock.vid,
                    .knockback = common::KnockbackSpec{
                        .velocity = sim::FxVec2{knockback_x, sim::Scalar::from_int(-2)},
                        .clear_velocity = !other_ent_const->impassable,
                        .clear_acceleration = !other_ent_const->impassable,
                        .thrown_by = holder != nullptr ? std::optional<VID>(holder->vid) : std::nullopt,
                        .thrown_immunity_timer = common::kThrownByImmunityDuration,
                        .proj_contact_damage_type = damage_type,
                        .proj_contact_damage_amount = 1,
                        .proj_contact_duration = other_ent_const->impassable
                            ? 0U
                            : common::kProjContactDuration,
                    },
                }
            );
        if (damage_result == common::DamageResult::None) {
            continue;
        }
        if (is_heavy_target && damage_result == common::DamageResult::Died) {
            (void)PlayWorldSoundEmitter(state, sim::ToRenderVec2(other_aabb.center()), audio_asset_ids::PotShatter);
        }
        if (Ent* const other_ent = state.ents.GetEntMut(other_ent_const->vid)) {
            result.sound_pos = other_ent->GetRenderCenter();
        } else {
            result.sound_pos = sim::ToRenderVec2(other_aabb.center());
        }
        result.hit_any = true;
    }

    return result;
}

StrikeOutcome ComputeMattockStrikeOutcome(
    const Ent& mattock,
    const Ent* holder,
    State& state,
    Audio& audio
) {
    if (holder == nullptr) {
        const sim::FxVec2 fallback = GetFallbackStrikePoint(mattock);
        const IVec2 tile_pos = state.stage.GetTileCoordAtWc(ToWorldPixelTrunc(fallback));
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

void TryApplyMattockStrike(std::size_t ent_idx, State& state, const Graphics& graphics, Audio& audio) {
    Ent& mattock = state.ents.ents[ent_idx];
    Ent* holder = nullptr;
    if (mattock.held_by_vid.has_value()) {
        holder = state.ents.GetEntMut(*mattock.held_by_vid);
    }

    const EntStrikeOutcome ent_outcome =
        TryStrikeEntsWithMattock(mattock, holder, state, graphics, audio);
    const StrikeOutcome tile_outcome = ComputeMattockStrikeOutcome(mattock, holder, state, audio);
    if (!ent_outcome.hit_any && tile_outcome.result == StrikeResult::Miss) {
        return;
    }

    const int direction = mattock.facing == Side::Left ? -1 : 1;
    const FVec2 effect_pos = tile_outcome.result != StrikeResult::Miss ? tile_outcome.sound_pos
                                                                      : ent_outcome.sound_pos;
    SpawnMattockImpactParticles(state, effect_pos, direction);
    (void)PlayWorldSoundEmitter(state, effect_pos, audio_asset_ids::Pickaxe);
    if (tile_outcome.result == StrikeResult::HitUnbreakable) {
        (void)PlayWorldSoundEmitter(state, tile_outcome.sound_pos, audio_asset_ids::UnbreakableHit);
        return;
    }
    if (tile_outcome.result == StrikeResult::Dug && ShouldBreakMattockAfterSuccessfulDig(mattock, state)) {
        mattock.marked_for_destruction = true;
    }
}

} // namespace

void OnUseAsMattock(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)state;
    (void)graphics;
    (void)audio;
    Ent& mattock = state.ents.ents[ent_idx];
    if (!mattock.use_state.pressed || IsSwinging(mattock)) {
        return;
    }

    SetAnim(mattock, aframe_ids::MattockSwing);
    mattock.aframe_animator.loop = false;
    mattock.counter_a = sim::ToSimScalar(kMattockStrikePending);

    if (mattock.use_state.source == AttachMode::None) {
        StopUsingEnt(mattock);
    }
}

void StepEntLogicAsMattock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    Ent& mattock = state.ents.ents[ent_idx];

    const Ent* holder = nullptr;
    if (mattock.held_by_vid.has_value()) {
        holder = state.ents.GetEnt(*mattock.held_by_vid);
    }
    AddMattockDebugAnnotations(mattock, holder, state, graphics);

    if (!IsSwinging(mattock)) {
        return;
    }

    if (mattock.counter_a > sim::Scalar::zero() && mattock.aframe_animator.current_frame > 0) {
        TryApplyMattockStrike(ent_idx, state, graphics, audio);
        mattock.counter_a = sim::Scalar::zero();
    }

    if (!mattock.aframe_animator.IsFinished()) {
        return;
    }

    SetAnim(mattock, aframe_ids::Mattock);
    mattock.aframe_animator.loop = true;
}

extern const EntSpec kMattockSpec{
    .type_ = EntType::Mattock,
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
    .counter_b = EntSpecCounter(kMattockGuaranteedDigs),
    .damage_vuln = DamageVuln::Immune,
    .on_use = OnUseAsMattock,
    .step_logic = StepEntLogicAsMattock,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Mattock),
};

} // namespace splonks::ents::mattock
