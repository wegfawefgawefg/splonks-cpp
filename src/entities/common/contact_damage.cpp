#include "entities/common/common.hpp"

#include "world_query.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHarmContactCooldownFrames = 8;
constexpr std::uint32_t kProjectileBodyImpactCooldownFrames = 60;

bool HasContactHarmAlignment(const Entity& source, const Entity& target) {
    return (source.alignment == Alignment::Ally && target.alignment == Alignment::Enemy) ||
           (source.alignment == Alignment::Enemy && target.alignment == Alignment::Ally) ||
           source.alignment == Alignment::Neutral;
}

bool CanApplyProjectileContact(const Entity& entity) {
    return entity.projectile_contact_timer > 0 && Length(entity.vel) >= 1.0F;
}

bool CanProjectileImpactWithoutDamage(const Entity& target) {
    return target.condition == EntityCondition::Stunned ||
           target.condition == EntityCondition::Dead;
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
    Vec2 velocity = source.vel;
    if (velocity.x == 0.0F) {
        const Vec2 delta = GetNearestWorldDelta(stage, source.GetCenter(), target.GetCenter());
        velocity.x = delta.x < 0.0F ? -2.0F : 2.0F;
    }
    if (velocity.y > -1.0F) {
        velocity.y = -1.0F;
    }

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
                if (other_entity->type_ == EntityType::Player) {
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
                        const DamageResult damage_result =
                            TryDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            ApplyKnockback(
                                *other_entity,
                                BuildBodyContactKnockback(entity, *other_entity, state.stage)
                            );
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

bool MaybeHurtAndStunOnContactAsProjectile(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (!CanApplyProjectileContact(entity)) {
        return false;
    }

    bool hit = false;
    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const std::vector<VID> search_results =
        QueryEntitiesInAabb(state, entity_aabb, entity.vid);
    for (const VID& vid : search_results) {
        const Entity* const other_entity = state.entity_manager.GetEntity(vid);
        if (other_entity == nullptr || other_entity->impassable) {
            continue;
        }
        if (TryApplyProjectileContactToEntity(entity_idx, vid.id, state, graphics, audio)) {
            hit = true;
        }
    }
    return hit;
}

void MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    MaybeHurtAndStunOnContact(entity_idx, state, graphics, audio);
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.condition != EntityCondition::Normal) {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, graphics, audio);
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
        MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(entity_idx, state, graphics, audio);
    } else {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, graphics, audio);
    }
}

void DieIfFootInSpikes(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    bool fell_into_spikes = false;
    if (entity.vel.y > 0.0F) {
        const IAABB iaabb = entity.GetAABB().AsIAABB();
        const bool override_tile_portion_check = entity.vel.y > 4.0F;
        const bool in_top_portion_of_tile = (iaabb.br.y % static_cast<int>(kTileSize)) < 4;
        if (in_top_portion_of_tile || override_tile_portion_check) {
            for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, iaabb.tl, iaabb.br)) {
                if (tile_query.tile != nullptr && *tile_query.tile == Tile::Spikes) {
                    fell_into_spikes = true;
                }
            }
        }
    }
    if (fell_into_spikes) {
        const DamageResult damage_result =
            TryDamageEntity(entity.vid.id, state, audio, DamageType::Spikes, 1);
        switch (damage_result) {
        case DamageResult::Hurt:
        case DamageResult::Died:
            entity.vel.x = 0.0F;
            audio.PlaySoundEffect(SoundEffect::AnimalCrush2);
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
    if (entity.thrown_by.has_value() && other_entity.vid == *entity.thrown_by) {
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

    const DamageResult damage_result =
        TryDamageEntity(
            other_entity_idx,
            state,
            audio,
            entity.projectile_contact_damage_type,
            entity.projectile_contact_damage_amount
        );
    switch (damage_result) {
    case DamageResult::Hurt:
    case DamageResult::Died: {
        Entity& other_entity_mut = state.entity_manager.entities[other_entity_idx];
        ApplyKnockback(
            other_entity_mut,
            BuildProjectileContactKnockback(entity, other_entity_mut, state.stage)
        );
        state.contact.AddProjectileBodyImpactCooldown(
            entity.vid,
            other_entity.vid,
            state.stage_frame,
            kProjectileBodyImpactCooldownFrames
        );
        if (entity.collide_sound.has_value()) {
            audio.PlaySoundEffect(*entity.collide_sound);
        }
        return true;
    }
    case DamageResult::None:
        if (CanProjectileImpactWithoutDamage(other_entity)) {
            Entity& other_entity_mut = state.entity_manager.entities[other_entity_idx];
            ApplyKnockback(
                other_entity_mut,
                BuildProjectileContactKnockback(entity, other_entity_mut, state.stage)
            );
            state.contact.AddProjectileBodyImpactCooldown(
                entity.vid,
                other_entity.vid,
                state.stage_frame,
                kProjectileBodyImpactCooldownFrames
            );
            if (entity.collide_sound.has_value()) {
                audio.PlaySoundEffect(*entity.collide_sound);
            }
            return true;
        }
        return false;
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
    StepStunTimer(entity_idx, state);
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        if (entity.hang_count > 0) {
            entity.hang_count -= 1;
        }
    }
    DoThrownByStep(entity_idx, state);
    ApplyHurtOnContact(entity_idx, state, graphics, audio);
    DieIfFootInSpikes(entity_idx, state, audio);
    (void)dt;
}

} // namespace splonks::entities::common
