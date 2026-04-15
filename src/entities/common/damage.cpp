#include "entities/common/common.hpp"

#include "on_damage_effects.hpp"

namespace splonks::entities::common {

namespace {

void EnterStunnedState(Entity& entity, State& state) {
    DropHeldItemFromEntity(entity, state);
    entity.condition = EntityCondition::Stunned;
    TrySetAnimation(entity, EntityDisplayState::Stunned);
    entity.stun_timer = kDefaultStunTimer;
}

std::optional<SoundEffect> GetCrushSoundEffect(EntityType type_) {
    switch (type_) {
    case EntityType::Player:
        return SoundEffect::AnimalCrush1;
    case EntityType::Bat:
        return SoundEffect::AnimalCrush2;
    case EntityType::Gold:
    case EntityType::GoldStack:
    case EntityType::GoldChunk:
    case EntityType::GoldNugget:
    case EntityType::GoldBar:
    case EntityType::GoldBars:
        return SoundEffect::MoneySmashed;
    case EntityType::Rock:
        return SoundEffect::Thud;
    default:
        return std::nullopt;
    }
}

void OnDeath(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const std::optional<SoundEffect> sound_effect =
        entity.stone ? std::optional<SoundEffect>(SoundEffect::PotShatter)
                     : entity.death_sound_effect;
    if (sound_effect.has_value()) {
        audio.PlaySoundEffect(*sound_effect);
    }
    if (entity.on_death != nullptr) {
        entity.on_death(entity_idx, state, audio);
    }
}

EntityDamageEffectResult ApplyDamageEffect(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (damage_applied) {
        DropHeldItemFromEntity(entity, state);
    }
    if (damage_applied && !entity.stone) {
        if (entity.damage_animation.has_value()) {
            SpawnDamageEffectAnimationBurst(*entity.damage_animation, entity.GetCenter(), state);
        }
        if (entity.damage_sound.has_value()) {
            audio.PlaySoundEffect(*entity.damage_sound);
        }
    }
    if (entity.on_damage != nullptr) {
        return entity.on_damage(entity_idx, state, audio, damage_type, amount, damage_applied);
    }
    return EntityDamageEffectResult::None;
}

} // namespace

void OnDeathAsExplosion(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    DoExplosion(entity_idx, entity.GetCenter(), 2.0F, state, audio);
    state.entity_manager.SetInactive(entity_idx);
}

void DieIfDead(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool entered_dead = entity.condition != EntityCondition::Dead && entity.health == 0;
    if (entity.health == 0) {
        if (entered_dead) {
            DropHeldItemFromEntity(entity, state);
        }
        entity.condition = EntityCondition::Dead;
        if (entered_dead && !entity.marked_for_destruction) {
            TrySetAnimation(entity, EntityDisplayState::Dead);
        }
    }
    if (entered_dead) {
        OnDeath(entity_idx, state, audio);
    }
}

DamageResult TryDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, false) ==
        EntityDamageEffectResult::Consumed) {
        return DamageResult::Hurt;
    }

    const DamageVulnerability v = entity.damage_vulnerability;
    if (v == DamageVulnerability::Immune) {
        return DamageResult::None;
    }
    bool can_damage = false;
    switch (entity.damage_vulnerability) {
    case DamageVulnerability::AttackingOnly:
        can_damage = damage_type == DamageType::Attack;
        break;
    case DamageVulnerability::BurningOnly:
        can_damage = damage_type == DamageType::Burn;
        break;
    case DamageVulnerability::CrushingOnly:
        can_damage = damage_type == DamageType::Crush;
        break;
    case DamageVulnerability::ExplosionOnly:
        can_damage = damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::CrushingAndSpikes:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes;
        break;
    case DamageVulnerability::CrushingSpikesAndExplosion:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes ||
                     damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::HeavyAttackOnly:
        can_damage = damage_type == DamageType::HeavyAttack;
        break;
    case DamageVulnerability::Vulnerable:
        can_damage = true;
        break;
    case DamageVulnerability::Immune:
        can_damage = false;
        break;
    case DamageVulnerability::AnthingExceptJumpOn:
        can_damage = damage_type != DamageType::JumpOn;
        break;
    }
    if (can_damage) {
        if (entity.stone && damage_type == DamageType::Explosion) {
            entity.health = 0;
            DieIfDead(entity_idx, state, audio);
            return DamageResult::Died;
        }
        bool do_damage_calculation = false;
        if (damage_type == DamageType::Crush) {
            entity.health = 0;
            (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
            DieIfDead(entity_idx, state, audio);
            if (!entity.active) {
                return DamageResult::Died;
            }
            if (const std::optional<SoundEffect> sound_effect = GetCrushSoundEffect(entity.type_)) {
                audio.PlaySoundEffect(*sound_effect);
            }
            entity.marked_for_destruction = true;
            return DamageResult::Died;
        }
        if (entity.condition == EntityCondition::Dead) {
            return DamageResult::None;
        } else {
            if (damage_type == DamageType::Spikes) {
                entity.health = 0;
                (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
                DieIfDead(entity_idx, state, audio);
                return DamageResult::Died;
            } else if (damage_type == DamageType::Explosion) {
                do_damage_calculation = true;
                if (entity.can_be_stunned && entity.condition != EntityCondition::Stunned) {
                    EnterStunnedState(entity, state);
                }
            } else if (entity.can_be_stunned) {
                if (entity.condition != EntityCondition::Stunned) {
                    EnterStunnedState(entity, state);
                    do_damage_calculation = true;
                } else {
                    return DamageResult::None;
                }
            } else {
                do_damage_calculation = true;
            }
        }
        if (do_damage_calculation) {
            if (entity.health > amount) {
                entity.health -= amount;
                (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
                return DamageResult::Hurt;
            }
            entity.health = 0;
            (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
            DieIfDead(entity_idx, state, audio);
            return DamageResult::Died;
        }
    }
    return DamageResult::None;
}

} // namespace splonks::entities::common
