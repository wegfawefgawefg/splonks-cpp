#include "entities/common/common.hpp"

#include "effects.hpp"
#include "tile_contact_data.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHarmContactCooldownFrames = 8;
constexpr std::uint32_t kProjectileBodyImpactCooldownFrames = 60;
constexpr std::uint32_t kTileOverlapEffectRefreshFrames = 2;
constexpr float kProjectileContactVelocityScale = 0.35F;

bool HasContactHarmAlignment(const Entity& source, const Entity& target) {
    return (source.alignment == Alignment::Ally && target.alignment == Alignment::Enemy) ||
           (source.alignment == Alignment::Enemy && target.alignment == Alignment::Ally) ||
           source.alignment == Alignment::Neutral;
}

bool CanApplyProjectileContact(const Entity& entity) {
    return entity.can_apply_projectile_contact && entity.projectile_contact_timer > 0 &&
           Length(entity.vel) >= 1.0F;
}

void ApplyTileOverlapEffects(std::size_t entity_idx, State& state) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, entity.GetAABB())) {
        if (tile_query.tile == nullptr) {
            continue;
        }
        const TileArchetype& tile_archetype = GetTileArchetype(*tile_query.tile);
        if (tile_archetype.effect_while_inside.has_value()) {
            (void)AddEffect(
                entity,
                *tile_archetype.effect_while_inside,
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
        const TileArchetype& fluid_archetype = GetTileArchetype(fluid_tile);
        if (fluid_archetype.effect_while_inside.has_value()) {
            (void)AddEffect(
                entity,
                *fluid_archetype.effect_while_inside,
                0,
                kTileOverlapEffectRefreshFrames
            );
        }
    }
}

bool CanProjectileImpactWithoutDamage(const Entity& target) {
    return target.condition == EntityCondition::Stunned ||
           target.condition == EntityCondition::Dead;
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

bool EntityIsMovingIntoSpike(const Entity& entity, TileRotation spike_rotation) {
    constexpr float kMinSpikeImpactSpeed = 0.01F;
    switch (spike_rotation) {
    case kTileRotation90:
        return entity.vel.x < -kMinSpikeImpactSpeed;
    case kTileRotation180:
        return entity.vel.y < -kMinSpikeImpactSpeed;
    case kTileRotation270:
        return entity.vel.x > kMinSpikeImpactSpeed;
    case kTileRotation0:
    default:
        return entity.vel.y > kMinSpikeImpactSpeed;
    }
}

KnockbackSpec BuildBodyContactKnockback(const Entity& source, const Entity& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, source.GetCenter(), target.GetCenter());
    const float direction = delta.x < 0.0F ? -1.0F : 1.0F;
    (void)target;
    return KnockbackSpec{
        .velocity = Vec2::New(1.0F * direction, -1.0F),
        .clear_velocity = true,
        .clear_acceleration = true,
    };
}

KnockbackSpec BuildProjectileContactKnockback(const Entity& source, const Entity& target, const Stage& stage) {
    (void)target;
    (void)stage;
    const Vec2 velocity = source.vel * kProjectileContactVelocityScale;

    return KnockbackSpec{
        .velocity = velocity,
        .clear_velocity = false,
        .clear_acceleration = true,
        .thrown_by = source.thrown_by,
        .thrown_immunity_timer = kThrownByImmunityDuration,
        .projectile_contact_damage_type = DamageType::Attack,
        .projectile_contact_damage_amount = 1,
        .projectile_contact_duration = kProjectileContactDuration,
    };
}

void MaybeHurtAndStunOnContact(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const Vec2 entity_pos = entity.pos;
    const EntityCondition condition = entity.condition;
    const bool hurt_on_contact = entity.hurt_on_contact;
    const std::optional<VID> thrown_by = entity.thrown_by;
    if (condition == EntityCondition::Normal && hurt_on_contact) {
        const std::vector<VID> search_results =
            QueryEntitiesInAabb(state, entity_aabb, entity_vid);
        std::vector<VID> results;
        for (const VID& vid : search_results) {
            const Entity& e = state.entity_manager.entities[vid.id];
            if (!e.impassable) {
                results.push_back(vid);
            }
        }
        for (const VID& vid : results) {
            if (thrown_by.has_value() && vid == *thrown_by) {
                continue;
            }
            if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
                const AABB other_aabb = GetNearestWorldAabb(
                    state.stage,
                    entity.GetCenter(),
                    GetContactAabbForEntity(*other_entity, graphics)
                );
                if (!AabbsIntersect(entity_aabb, other_aabb)) {
                    continue;
                }
                if (IsPlayerLikeEntityType(other_entity->type_)) {
                    const AABB player_aabb = other_aabb;
                    const AABB player_foot = {
                        .tl = Vec2::New(player_aabb.tl.x, player_aabb.br.y - 4.0F),
                        .br = player_aabb.br,
                    };
                    if (entity_pos.x >= player_foot.tl.x && entity_pos.x <= player_foot.br.x &&
                        entity_pos.y >= player_foot.tl.y && entity_pos.y <= player_foot.br.y) {
                        continue;
                    }
                }
                if (other_entity->can_collide) {
                    if (HasContactHarmAlignment(entity, *other_entity)) {
                        if (state.contact.HasInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
                                InteractionCooldownKind::Harm
                            )) {
                            continue;
                        }
                        const DamageResult damage_result = TryHitEntity(
                            other_entity->vid.id,
                            state,
                            audio,
                            DamageType::Attack,
                            1,
                            HitOptions{
                                .source_vid = entity.vid,
                                .knockback = BuildBodyContactKnockback(entity, *other_entity, state.stage),
                            }
                        );
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            state.contact.AddInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
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
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.hurt_on_contact) {
        MaybeHurtAndStunOnContact(entity_idx, state, graphics, audio);
    }
}

void DieIfFootInSpikes(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (GetModifiedEffectValue(entity, EffectModifierTarget::SpikeDamageTaken, 1.0F) <= 0.0F) {
        return;
    }
    if (entity.IsClimbing() || entity.IsHanging()) {
        return;
    }

    bool hit_spikes = false;
    {
        const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
        const IAABB iaabb = entity_aabb.AsIAABB();
        const bool override_tile_portion_check = Length(entity.vel) > 4.0F;
        const bool in_top_portion_of_tile = (iaabb.br.y % static_cast<int>(kTileSize)) < 4;
        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, iaabb.tl, iaabb.br)) {
            if (tile_query.tile == nullptr || *tile_query.tile != Tile::Spikes) {
                continue;
            }
            const TileRotation spike_rotation = GetTileRotationForQuery(state.stage, tile_query);
            if (!EntityIsMovingIntoSpike(entity, spike_rotation)) {
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
                entity.GetCenter()
            );
            if (AabbsIntersect(entity_aabb, spike_cbox_aabb)) {
                hit_spikes = true;
            }
        }
    }
    if (hit_spikes) {
        const DamageResult damage_result =
            TryDamageEntity(entity.vid.id, state, audio, DamageType::Spikes, 1);
        switch (damage_result) {
        case DamageResult::Hurt:
        case DamageResult::Died:
            entity.vel.x = 0.0F;
            (void)PlayEntityCenterSoundEmitter(state, entity, audio_asset_ids::AnimalCrush2);
            break;
        case DamageResult::None:
            break;
        }
    }
}

} // namespace

bool TryApplyProjectileContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& entity = state.entity_manager.entities[entity_idx];
    const Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!entity.active || !other_entity.active || !CanApplyProjectileContact(entity)) {
        return false;
    }
    if (!entity.can_collide) {
        return false;
    }
    if (entity.held_by_vid.has_value()) {
        return false;
    }
    if (entity.thrown_by.has_value() && other_entity.vid == *entity.thrown_by) {
        return false;
    }
    if (!other_entity.can_be_hit) {
        return false;
    }
    if (!other_entity.can_receive_projectile_contact) {
        return false;
    }
    if (!other_entity.can_collide) {
        return false;
    }
    if (state.contact.HasProjectileBodyImpactCooldown(entity.vid, other_entity.vid)) {
        return false;
    }

    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const AABB other_aabb = GetNearestWorldAabb(
        state.stage,
        entity.GetCenter(),
        GetContactAabbForEntity(other_entity, graphics)
    );
    if (!AabbsIntersect(entity_aabb, other_aabb)) {
        return false;
    }

    const KnockbackSpec knockback = BuildProjectileContactKnockback(entity, other_entity, state.stage);
    const DamageResult damage_result =
        entity.projectile_contact_damage_amount > 0
            ? TryHitEntity(
                  other_entity_idx,
                  state,
                  audio,
                  entity.projectile_contact_damage_type,
                  entity.projectile_contact_damage_amount,
                  HitOptions{
                      .source_vid = entity.vid,
                      .knockback = knockback,
                      .knockback_on_no_damage = true,
                  }
              )
            : DamageResult::None;
    switch (damage_result) {
    case DamageResult::Hurt:
    case DamageResult::Died:
    case DamageResult::None: {
        Entity& other_entity_mut = state.entity_manager.entities[other_entity_idx];
        if (entity.projectile_contact_damage_amount == 0 &&
            !other_entity_mut.held_by_vid.has_value() &&
            other_entity_mut.attachment_mode == AttachmentMode::None) {
            ApplyKnockback(other_entity_mut, knockback);
        }
        state.contact.AddProjectileBodyImpactCooldown(
            entity.vid,
            other_entity.vid,
            state.stage_frame,
            kProjectileBodyImpactCooldownFrames
        );
        if (entity.collide_sound.has_value()) {
            (void)PlayEntityCenterSoundEmitter(state, entity, *entity.collide_sound);
        }
        return true;
    }
    }

    return false;
}

void CommonStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    DieIfDead(entity_idx, state, audio);
    StepEffectTimers(state.entity_manager.entities[entity_idx]);
    ApplyTileOverlapEffects(entity_idx, state);
    StepStunTimer(entity_idx, state);
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        if (entity.hang_count > 0) {
            entity.hang_count -= 1;
        }
    }
    DoThrownByStep(entity_idx, state);
    ApplyHurtOnContact(entity_idx, state, graphics, audio);
    DieIfFootInSpikes(entity_idx, state, graphics, audio);
    (void)dt;
}

} // namespace splonks::entities::common
