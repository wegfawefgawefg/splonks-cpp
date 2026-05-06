#include "entities/machete.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "entities/common/common.hpp"
#include "entities/sac_altar.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace splonks::entities::machete {

namespace {

constexpr float kMacheteStrikePending = 1.0F;
constexpr unsigned int kMacheteDamage = 8;
constexpr std::int32_t kThrownKillFavor = 1;
constexpr std::int32_t kCorpseCutFavor = 1;

bool IsSwinging(const Entity& machete) {
    return machete.frame_data_animator.animation_id == frame_data_ids::KnifeSwing;
}

std::int32_t GetPendingFavor(const Entity& machete) {
    return std::max(0, static_cast<std::int32_t>(machete.counter_b));
}

void AddPendingFavor(Entity& machete, std::int32_t amount) {
    machete.counter_b = static_cast<float>(GetPendingFavor(machete) + std::max(0, amount));
}

void ClearPendingFavor(Entity& machete) {
    machete.counter_b = 0.0F;
}

Vec2 GetVictimEffectPos(const Entity& victim, const Graphics& graphics) {
    const AABB victim_aabb = common::GetContactAabbForEntity(victim, graphics);
    return Vec2::New(
        (victim_aabb.tl.x + victim_aabb.br.x) * 0.5F,
        victim_aabb.br.y - 2.0F
    );
}

bool CanMacheteHitEntity(const Entity& machete, const Entity* holder, const Entity& other_entity) {
    if (!other_entity.active || !other_entity.can_collide) {
        return false;
    }
    if (other_entity.impassable || other_entity.condition == EntityCondition::Dead) {
        return false;
    }
    if (other_entity.vid == machete.vid) {
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

bool CanCarveCorpse(const Entity& victim) {
    return victim.active && victim.condition == EntityCondition::Dead &&
           entities::sac_altar::GetSacrificeFavorValue(victim).has_value();
}

void CarveCorpse(Entity& machete, Entity& victim, State& state, const Graphics& graphics, Audio& audio) {
    const Vec2 effect_pos = GetVictimEffectPos(victim, graphics);
    common::DropHeldItemFromEntity(victim, state);
    common::ReleaseEntityFromHolder(victim, state);
    victim.marked_for_destruction = true;
    state.entity_manager.SetInactive(victim.vid.id);
    state.UpdateSidForEntity(victim.vid.id, graphics);
    AddPendingFavor(machete, kCorpseCutFavor);
    entities::sac_altar::SpawnSacrificeGainEffects(state, audio, effect_pos);
}

void HandleHeldKillFavor(Entity& machete, const Entity& victim_before_damage, State& state, const Graphics& graphics, Audio& audio) {
    const std::optional<std::int32_t> favor = entities::sac_altar::GetLivingSacrificeFavorValue(victim_before_damage);
    if (!favor.has_value()) {
        return;
    }
    AddPendingFavor(machete, *favor);
    entities::sac_altar::SpawnSacrificeGainEffects(state, audio, GetVictimEffectPos(victim_before_damage, graphics));
}

void HandleThrownKillFavor(Entity& machete, const Entity& victim, State& state, const Graphics& graphics, Audio& audio) {
    AddPendingFavor(machete, kThrownKillFavor);
    entities::sac_altar::SpawnSacrificeGainEffects(state, audio, GetVictimEffectPos(victim, graphics));
}

bool TryDepositFavorWhileGroundedOnSacAltar(
    Entity& machete,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (machete.held_by_vid.has_value() || !machete.grounded || GetPendingFavor(machete) <= 0) {
        return false;
    }

    const AABB feet = machete.GetFeet();
    for (const VID& other_vid : QueryEntitiesInAabb(state, feet, machete.vid)) {
        Entity* const other_entity = state.entity_manager.GetEntityMut(other_vid);
        if (other_entity == nullptr || !other_entity->active || other_entity->type_ != EntityType::SacAltar) {
            continue;
        }

        const AABB altar_aabb = GetNearestWorldAabb(
            state.stage,
            machete.GetCenter(),
            common::GetContactAabbForEntity(*other_entity, graphics)
        );
        if (!AabbsIntersect(feet, altar_aabb)) {
            continue;
        }

        if (entities::sac_altar::TryDepositStoredFavor(
                *other_entity,
                GetPendingFavor(machete),
                state,
                graphics,
                audio
            )) {
            ClearPendingFavor(machete);
            return true;
        }
    }

    return false;
}

void TryApplyMacheteStrike(std::size_t entity_idx, State& state, const Graphics& graphics, Audio& audio) {
    Entity& machete = state.entity_manager.entities[entity_idx];
    Entity* holder = nullptr;
    if (machete.held_by_vid.has_value()) {
        holder = state.entity_manager.GetEntityMut(*machete.held_by_vid);
    }

    const AABB strike_aabb = common::GetContactAabbForEntity(machete, graphics);
    for (const VID& other_vid : QueryEntitiesInAabb(state, strike_aabb, machete.vid)) {
        Entity* const other_entity = state.entity_manager.GetEntityMut(other_vid);
        if (other_entity == nullptr) {
            continue;
        }

        const AABB other_aabb = GetNearestWorldAabb(
            state.stage,
            machete.GetCenter(),
            common::GetContactAabbForEntity(*other_entity, graphics)
        );
        if (!AabbsIntersect(strike_aabb, other_aabb)) {
            continue;
        }

        if (CanCarveCorpse(*other_entity)) {
            CarveCorpse(machete, *other_entity, state, graphics, audio);
            continue;
        }

        if (!CanMacheteHitEntity(machete, holder, *other_entity)) {
            continue;
        }

        const Entity victim_before_damage = *other_entity;
        const float knockback_x = machete.facing == LeftOrRight::Left ? -3.5F : 3.5F;
        const common::DamageResult damage_result =
            common::TryHitEntity(
                other_entity->vid.id,
                state,
                audio,
                DamageType::Attack,
                kMacheteDamage,
                common::HitOptions{
                    .source_vid = machete.vid,
                    .knockback = common::KnockbackSpec{
                        .velocity = Vec2::New(knockback_x, -1.5F),
                        .clear_velocity = true,
                        .clear_acceleration = true,
                        .thrown_by = holder != nullptr ? std::optional<VID>(holder->vid) : std::nullopt,
                        .thrown_immunity_timer = common::kThrownByImmunityDuration,
                        .projectile_contact_damage_type = DamageType::Attack,
                        .projectile_contact_damage_amount = kMacheteDamage,
                        .projectile_contact_duration = common::kProjectileContactDuration,
                    },
                    .allow_remote_player_target = true,
                }
            );
        if (damage_result == common::DamageResult::None) {
            continue;
        }

        if (damage_result == common::DamageResult::Died) {
            HandleHeldKillFavor(machete, victim_before_damage, state, graphics, audio);
        }

        if (damage_result == common::DamageResult::Requested) {
            continue;
        }

    }
}

common::ContactResolution OnEntityContactAsMachete(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr || context.phase != common::ContactPhase::SweptEntered) {
        return common::ContactResolution{};
    }
    if (entity_idx >= state.entity_manager.entities.size() || other_entity_idx >= state.entity_manager.entities.size()) {
        return common::ContactResolution{};
    }

    Entity& machete = state.entity_manager.entities[entity_idx];
    Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!machete.active || machete.type_ != EntityType::Machete || !other_entity.active) {
        return common::ContactResolution{};
    }

    if (machete.projectile_contact_timer == 0 || machete.held_by_vid.has_value()) {
        return common::ContactResolution{};
    }

    if (other_entity.type_ == EntityType::Cobweb) {
        (void)common::TryDamageEntity(other_entity_idx, state, *audio, DamageType::Attack, kMacheteDamage);
        return common::ContactResolution{.stop_sweep = true};
    }

    if (CanCarveCorpse(other_entity) && other_entity.last_condition == EntityCondition::Dead) {
        CarveCorpse(machete, other_entity, state, *graphics, *audio);
        return common::ContactResolution{.stop_sweep = true};
    }

    if (other_entity.condition == EntityCondition::Dead && other_entity.last_condition != EntityCondition::Dead &&
        entities::sac_altar::GetSacrificeFavorValue(other_entity).has_value()) {
        HandleThrownKillFavor(machete, other_entity, state, *graphics, *audio);
    }

    return common::ContactResolution{};
}

} // namespace

void OnUseAsMachete(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)state;
    (void)graphics;
    (void)audio;
    Entity& machete = state.entity_manager.entities[entity_idx];
    if (!machete.use_state.pressed || IsSwinging(machete)) {
        return;
    }

    SetAnimation(machete, frame_data_ids::KnifeSwing);
    machete.frame_data_animator.loop = false;
    machete.counter_a = kMacheteStrikePending;
    (void)PlayEntityCenterSoundEmitter(state, machete, audio_asset_ids::Throw);

    if (machete.use_state.source == AttachmentMode::None) {
        StopUsingEntity(machete);
    }
}

void StepEntityLogicAsMachete(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    Entity& machete = state.entity_manager.entities[entity_idx];
    TryDepositFavorWhileGroundedOnSacAltar(machete, state, graphics, audio);
    if (!IsSwinging(machete)) {
        return;
    }

    if (machete.counter_a > 0.0F && machete.frame_data_animator.current_frame > 0) {
        TryApplyMacheteStrike(entity_idx, state, graphics, audio);
        machete.counter_a = 0.0F;
    }

    if (!machete.frame_data_animator.IsFinished()) {
        return;
    }

    SetAnimation(machete, frame_data_ids::Knife);
    machete.frame_data_animator.loop = true;
}

extern const EntityArchetype kMacheteArchetype{
    .type_ = EntityType::Machete,
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
    .projectile_contact_damage_type = DamageType::Attack,
    .projectile_contact_damage_amount = kMacheteDamage,
    .on_use = OnUseAsMachete,
    .step_logic = StepEntityLogicAsMachete,
    .on_entity_contact = OnEntityContactAsMachete,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Knife),
};

} // namespace splonks::entities::machete
