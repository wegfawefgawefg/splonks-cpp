#include "ents/common/common.hpp"

#include "effects.hpp"
#include "tile_contact_data.hpp"
#include "tile_spec.hpp"
#include "world_query.hpp"

#include <vector>

namespace splonks::ents::common {

namespace {

constexpr std::uint32_t kHarmContactCooldownFrames = 8;
constexpr std::uint32_t kProjBodyImpactCooldownFrames = 60;
constexpr std::uint32_t kTileOverlapEffectRefreshFrames = 2;
constexpr float kProjContactVelocityScale = 0.35F;

bool HasContactHarmAlignment(const Ent& source, const Ent& target) {
    return (source.alignment == Alignment::Ally && target.alignment == Alignment::Enemy) ||
           (source.alignment == Alignment::Enemy && target.alignment == Alignment::Ally) ||
           source.alignment == Alignment::Neutral;
}

bool CanApplyProjContact(const Ent& ent) {
    return ent.can_apply_proj_contact && ent.proj_contact_timer > 0 &&
           Length(ent.vel) >= 1.0F;
}

void ApplyTileOverlapEffects(std::size_t ent_idx, State& state) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, ent.GetAABB())) {
        if (tile_query.tile == nullptr) {
            continue;
        }
        const TileSpec& tile_spec = GetTileSpec(*tile_query.tile);
        if (tile_spec.effect_while_inside.has_value()) {
            (void)AddEffect(
                ent,
                *tile_spec.effect_while_inside,
                0,
                kTileOverlapEffectRefreshFrames
            );
        }

        if (!state.stage.IsTileCoordInside(tile_query.tile_pos.x, tile_query.tile_pos.y)) {
            continue;
        }
        const Tile fluid_tile = state.stage.GetFluidTile(
            static_cast<unsigned int>(tile_query.tile_pos.x),
            static_cast<unsigned int>(tile_query.tile_pos.y)
        );
        if (state.stage.GetFluidAmount(
                static_cast<unsigned int>(tile_query.tile_pos.x),
                static_cast<unsigned int>(tile_query.tile_pos.y)
            ) < state.settings.fluid.render_cutoff_amount) {
            continue;
        }
        const TileSpec& fluid_spec = GetTileSpec(fluid_tile);
        if (fluid_spec.effect_while_inside.has_value()) {
            (void)AddEffect(
                ent,
                *fluid_spec.effect_while_inside,
                0,
                kTileOverlapEffectRefreshFrames
            );
        }
    }
}

bool CanProjImpactWithoutDamage(const Ent& target) {
    return target.condition == EntCondition::Stunned ||
           target.condition == EntCondition::Dead;
}

AABB GetTileContactCboxWorldAabb(const Stage& stage, const WorldTileQueryResult& tile_query,
                                 const TileContactData& tile_contact_data, const Vec2& anchor) {
    FrameRect cbox = tile_contact_data.cbox;
    const TileRotation rotation = GetTileRotationForQuery(stage, tile_query);
    constexpr int kTileSizePx = static_cast<int>(kTileSize);
    switch (rotation) {
    case kTileRotation90:
        cbox = FrameRect{
            .x = kTileSizePx - (tile_contact_data.cbox.y + tile_contact_data.cbox.h),
            .y = tile_contact_data.cbox.x,
            .w = tile_contact_data.cbox.h,
            .h = tile_contact_data.cbox.w,
        };
        break;
    case kTileRotation180:
        cbox = FrameRect{
            .x = kTileSizePx - (tile_contact_data.cbox.x + tile_contact_data.cbox.w),
            .y = kTileSizePx - (tile_contact_data.cbox.y + tile_contact_data.cbox.h),
            .w = tile_contact_data.cbox.w,
            .h = tile_contact_data.cbox.h,
        };
        break;
    case kTileRotation270:
        cbox = FrameRect{
            .x = tile_contact_data.cbox.y,
            .y = kTileSizePx - (tile_contact_data.cbox.x + tile_contact_data.cbox.w),
            .w = tile_contact_data.cbox.h,
            .h = tile_contact_data.cbox.w,
        };
        break;
    case kTileRotation0:
    default:
        break;
    }
    const Vec2 tile_tl = ToVec2(tile_query.tile_pos) * static_cast<float>(kTileSize);
    const AABB cbox_aabb = AABB::New(
        tile_tl + Vec2::New(static_cast<float>(cbox.x), static_cast<float>(cbox.y)),
        tile_tl + Vec2::New(
            static_cast<float>(cbox.x + cbox.w - 1),
            static_cast<float>(cbox.y + cbox.h - 1)
        )
    );
    return GetNearestWorldAabb(stage, anchor, cbox_aabb);
}

bool HasAuthoredTileCbox(const TileContactData& tile_contact_data) {
    return tile_contact_data.cbox.w > 0 && tile_contact_data.cbox.h > 0;
}

bool EntIsMovingIntoSpike(const Ent& ent, TileRotation spike_rotation) {
    constexpr float kMinSpikeImpactSpeed = 0.01F;
    switch (spike_rotation) {
    case kTileRotation90:
        return ent.vel.x < -kMinSpikeImpactSpeed;
    case kTileRotation180:
        return ent.vel.y < -kMinSpikeImpactSpeed;
    case kTileRotation270:
        return ent.vel.x > kMinSpikeImpactSpeed;
    case kTileRotation0:
    default:
        return ent.vel.y > kMinSpikeImpactSpeed;
    }
}

KnockbackSpec BuildBodyContactKnockback(const Ent& source, const Ent& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, source.GetCenter(), target.GetCenter());
    const float direction = delta.x < 0.0F ? -1.0F : 1.0F;
    (void)target;
    return KnockbackSpec{
        .velocity = Vec2::New(1.0F * direction, -1.0F),
        .clear_velocity = true,
        .clear_acceleration = true,
    };
}

KnockbackSpec BuildProjContactKnockback(const Ent& source, const Ent& target, const Stage& stage) {
    (void)target;
    (void)stage;
    const Vec2 velocity = source.vel * kProjContactVelocityScale;

    return KnockbackSpec{
        .velocity = velocity,
        .clear_velocity = false,
        .clear_acceleration = true,
        .thrown_by = source.thrown_by,
        .thrown_immunity_timer = kThrownByImmunityDuration,
        .proj_contact_damage_type = DamageType::Attack,
        .proj_contact_damage_amount = 1,
        .proj_contact_duration = kProjContactDuration,
    };
}

void MaybeHurtAndStunOnContact(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Ent& ent = state.ents.ents[ent_idx];
    const VID ent_vid = ent.vid;
    const AABB ent_aabb = GetContactAabbForEnt(ent, graphics);
    const Vec2 ent_pos = ent.pos;
    const EntCondition condition = ent.condition;
    const bool hurt_on_contact = ent.hurt_on_contact;
    const std::optional<VID> thrown_by = ent.thrown_by;
    if (condition == EntCondition::Normal && hurt_on_contact) {
        const std::vector<VID> search_results =
            QueryEntsInAabb(state, ent_aabb, ent_vid);
        std::vector<VID> results;
        for (const VID& vid : search_results) {
            const Ent& e = state.ents.ents[vid.id];
            if (!e.impassable) {
                results.push_back(vid);
            }
        }
        for (const VID& vid : results) {
            if (thrown_by.has_value() && vid == *thrown_by) {
                continue;
            }
            if (Ent* const other_ent = state.ents.GetEntMut(vid)) {
                const AABB other_aabb = GetNearestWorldAabb(
                    state.stage,
                    ent.GetCenter(),
                    GetContactAabbForEnt(*other_ent, graphics)
                );
                if (!AabbsIntersect(ent_aabb, other_aabb)) {
                    continue;
                }
                if (IsPlayerLikeEntType(other_ent->type_)) {
                    const AABB player_aabb = other_aabb;
                    const AABB player_foot = {
                        .tl = Vec2::New(player_aabb.tl.x, player_aabb.br.y - 4.0F),
                        .br = player_aabb.br,
                    };
                    if (ent_pos.x >= player_foot.tl.x && ent_pos.x <= player_foot.br.x &&
                        ent_pos.y >= player_foot.tl.y && ent_pos.y <= player_foot.br.y) {
                        continue;
                    }
                }
                if (other_ent->can_collide) {
                    if (HasContactHarmAlignment(ent, *other_ent)) {
                        if (state.contact.HasInteractionCooldown(
                                ent.vid,
                                other_ent->vid,
                                InteractionCooldownKind::Harm
                            )) {
                            continue;
                        }
                        const DamageResult damage_result = TryHitEnt(
                            other_ent->vid.id,
                            state,
                            audio,
                            DamageType::Attack,
                            1,
                            HitOptions{
                                .source_vid = ent.vid,
                                .knockback = BuildBodyContactKnockback(ent, *other_ent, state.stage),
                            }
                        );
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            state.contact.AddInteractionCooldown(
                                ent.vid,
                                other_ent->vid,
                                InteractionCooldownKind::Harm,
                                state.stage_frame,
                                kHarmContactCooldownFrames
                            );
                            break;
                        case DamageResult::None:
                            break;
                        }
                    }
                }
            }
        }
    }
}

void ApplyHurtOnContact(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Ent& ent = state.ents.ents[ent_idx];
    if (ent.hurt_on_contact) {
        MaybeHurtAndStunOnContact(ent_idx, state, graphics, audio);
    }
}

void DieIfFootInSpikes(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    Ent& ent = state.ents.ents[ent_idx];
    if (GetModifiedEffectValue(ent, EffectModifierTarget::SpikeDamageTaken, 1.0F) <= 0.0F) {
        return;
    }
    if (ent.IsClimbing() || ent.IsHanging()) {
        return;
    }

    bool hit_spikes = false;
    {
        const AABB ent_aabb = GetContactAabbForEnt(ent, graphics);
        const IAABB iaabb = ent_aabb.AsIAABB();
        const bool override_tile_portion_check = Length(ent.vel) > 4.0F;
        const bool in_top_portion_of_tile = (iaabb.br.y % static_cast<int>(kTileSize)) < 4;
        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, iaabb.tl, iaabb.br)) {
            if (tile_query.tile == nullptr || *tile_query.tile != Tile::Spikes) {
                continue;
            }
            const TileRotation spike_rotation = GetTileRotationForQuery(state.stage, tile_query);
            if (!EntIsMovingIntoSpike(ent, spike_rotation)) {
                continue;
            }

            const TileContactData* const tile_contact_data =
                GetTileContactData(graphics.tile_contact_db, *tile_query.tile, tile_query.tile_pos);
            if (tile_contact_data == nullptr || !HasAuthoredTileCbox(*tile_contact_data)) {
                if (spike_rotation != kTileRotation0 || in_top_portion_of_tile ||
                    override_tile_portion_check) {
                    hit_spikes = true;
                }
                continue;
            }

            const AABB spike_cbox_aabb = GetTileContactCboxWorldAabb(
                state.stage,
                tile_query,
                *tile_contact_data,
                ent.GetCenter()
            );
            if (AabbsIntersect(ent_aabb, spike_cbox_aabb)) {
                hit_spikes = true;
            }
        }
    }
    if (hit_spikes) {
        const DamageResult damage_result =
            TryDamageEnt(ent.vid.id, state, audio, DamageType::Spikes, 1);
        switch (damage_result) {
        case DamageResult::Hurt:
        case DamageResult::Died:
            ent.vel.x = 0.0F;
            (void)PlayEntCenterSoundEmitter(state, ent, audio_asset_ids::AnimalCrush2);
            break;
        case DamageResult::None:
            break;
        }
    }
}

} // namespace

bool TryApplyProjContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& ent = state.ents.ents[ent_idx];
    const Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!ent.active || !other_ent.active || !CanApplyProjContact(ent)) {
        return false;
    }
    if (!ent.can_collide) {
        return false;
    }
    if (ent.held_by_vid.has_value()) {
        return false;
    }
    if (ent.thrown_by.has_value() && other_ent.vid == *ent.thrown_by) {
        return false;
    }
    if (!other_ent.can_be_hit) {
        return false;
    }
    if (!other_ent.can_receive_proj_contact) {
        return false;
    }
    if (!other_ent.can_collide) {
        return false;
    }
    if (state.contact.HasProjBodyImpactCooldown(ent.vid, other_ent.vid)) {
        return false;
    }

    const AABB ent_aabb = GetContactAabbForEnt(ent, graphics);
    const AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        ent.GetCenter(),
        GetContactAabbForEnt(other_ent, graphics)
    );
    if (!AabbsIntersect(ent_aabb, other_aabb)) {
        return false;
    }

    const KnockbackSpec knockback = BuildProjContactKnockback(ent, other_ent, state.stage);
    const DamageResult damage_result =
        ent.proj_contact_damage_amount > 0
            ? TryHitEnt(
                  other_ent_idx,
                  state,
                  audio,
                  ent.proj_contact_damage_type,
                  ent.proj_contact_damage_amount,
                  HitOptions{
                      .source_vid = ent.vid,
                      .knockback = knockback,
                      .knockback_on_no_damage = true,
                  }
              )
            : DamageResult::None;
    switch (damage_result) {
    case DamageResult::Hurt:
    case DamageResult::Died:
    case DamageResult::None: {
        Ent& other_ent_mut = state.ents.ents[other_ent_idx];
        if (ent.proj_contact_damage_amount == 0 &&
            !other_ent_mut.held_by_vid.has_value() &&
            other_ent_mut.attach_mode == AttachMode::None) {
            ApplyKnockback(other_ent_mut, knockback);
        }
        state.contact.AddProjBodyImpactCooldown(
            ent.vid,
            other_ent.vid,
            state.stage_frame,
            kProjBodyImpactCooldownFrames
        );
        if (ent.collide_sound.has_value()) {
            (void)PlayEntCenterSoundEmitter(state, ent, *ent.collide_sound);
        }
        return true;
    }
    }

    return false;
}

void CommonStep(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    DieIfDead(ent_idx, state, audio);
    StepEffectTimers(state.ents.ents[ent_idx]);
    ApplyTileOverlapEffects(ent_idx, state);
    StepStunTimer(ent_idx, state);
    {
        Ent& ent = state.ents.ents[ent_idx];
        if (ent.hang_count > 0) {
            ent.hang_count -= 1;
        }
    }
    DoThrownByStep(ent_idx, state);
    ApplyHurtOnContact(ent_idx, state, graphics, audio);
    DieIfFootInSpikes(ent_idx, state, graphics, audio);
    (void)dt;
}

} // namespace splonks::ents::common
