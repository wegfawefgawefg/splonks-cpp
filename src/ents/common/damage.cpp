#include "ents/common/common.hpp"

#include "on_damage_effects.hpp"
#include "world_ops.hpp"

namespace splonks::ents::common {

namespace {

void EnterStunnedState(Ent& ent, State& state) {
    if (ent.held_by_vid.has_value() || ent.attach_mode != AttachMode::None) {
        ReleaseEntFromHolder(ent, state);
    }
    ent.condition = EntCondition::Stunned;
    TrySetAnim(ent, EntDisplayState::Stunned);
    ent.stun_timer = kDefaultStunTimer;
    DropHeldItemFromEnt(ent, state);
}

std::optional<AudioAssetId> GetCrushAudioAssetId(EntType type_) {
    switch (type_) {
    case EntType::Player:
        return audio_asset_ids::AnimalCrush1;
    case EntType::Bat:
        return audio_asset_ids::AnimalCrush2;
    case EntType::Gold:
    case EntType::GoldStack:
    case EntType::GoldChunk:
    case EntType::GoldNugget:
    case EntType::GoldBar:
    case EntType::GoldBars:
        return audio_asset_ids::MoneySmashed;
    case EntType::Rock:
        return audio_asset_ids::Thud;
    default:
        return std::nullopt;
    }
}

void OnDeath(std::size_t ent_idx, State& state, Audio& audio) {
    Ent& ent = state.ents.ents[ent_idx];
    ApplyEffectHookToAll(
        state,
        &audio,
        EffectHookContext{
            .type = EffectHookType::Death,
            .target_vid = ent.vid,
            .world_pos = ent.GetCenter(),
        }
    );
    if (ent.health > 0 && ent.condition != EntCondition::Dead) {
        return;
    }

    const std::optional<AudioAssetId> sound_effect =
        ent.stone ? std::optional<AudioAssetId>(audio_asset_ids::PotShatter)
                     : ent.death_sound;
    if (sound_effect.has_value()) {
        (void)PlayEntCenterSoundEmitter(state, ent, *sound_effect);
    }
    if (ent.on_death != nullptr) {
        ent.on_death(ent_idx, state, audio);
    }
}

EntDamageEffectResult ApplyDamageEffect(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    Ent& ent = state.ents.ents[ent_idx];
    if (damage_applied) {
        DropHeldItemFromEnt(ent, state);
    }
    if (damage_applied && !ent.stone) {
        if (ent.damage_anim.has_value()) {
            SpawnDamageEffectAnimBurst(*ent.damage_anim, ent.GetCenter(), state);
        }
        if (ent.damage_sound.has_value()) {
            (void)PlayEntCenterSoundEmitter(state, ent, *ent.damage_sound);
        }
    }
    if (ent.on_damage != nullptr) {
        return ent.on_damage(ent_idx, state, audio, damage_type, amount, damage_applied);
    }
    return EntDamageEffectResult::None;
}

} // namespace

void OnDeathAsExplosion(std::size_t ent_idx, State& state, Audio& audio) {
    Ent& ent = state.ents.ents[ent_idx];
    DoExplosion(ent_idx, ent.GetCenter(), 2.0F, 6.0F, state, audio);
    (void)world_ops::DeactivateEnt(state, ent.vid);
}

void DieIfDead(std::size_t ent_idx, State& state, Audio& audio) {
    Ent& ent = state.ents.ents[ent_idx];
    const bool entered_dead = ent.condition != EntCondition::Dead && ent.health == 0;
    if (ent.health == 0) {
        if (entered_dead) {
            ReleaseEntFromHolder(ent, state);
            DropHeldItemFromEnt(ent, state);
        }
        ent.condition = EntCondition::Dead;
        if (entered_dead && !ent.marked_for_destruction) {
            TrySetAnim(ent, EntDisplayState::Dead);
        }
    }
    if (entered_dead) {
        OnDeath(ent_idx, state, audio);
    }
}

bool DeathWasConsumed(const Ent& ent) {
    return ent.health > 0 && ent.condition != EntCondition::Dead;
}

bool CanEntTakeDamageType(const Ent& ent, DamageType damage_type) {
    switch (ent.damage_vuln) {
    case DamageVuln::AttackingOnly:
        return damage_type == DamageType::Attack ||
               damage_type == DamageType::IgnitingAttack;
    case DamageVuln::BurningOnly:
        return damage_type == DamageType::Burn;
    case DamageVuln::CrushingOnly:
        return damage_type == DamageType::Crush;
    case DamageVuln::ExplosionOnly:
        return damage_type == DamageType::Explosion;
    case DamageVuln::CrushingAndSpikes:
        return damage_type == DamageType::Crush || damage_type == DamageType::Spikes;
    case DamageVuln::CrushingSpikesAndExplosion:
        return damage_type == DamageType::Crush || damage_type == DamageType::Spikes ||
               damage_type == DamageType::Explosion;
    case DamageVuln::HeavyAttackOnly:
        return damage_type == DamageType::HeavyAttack;
    case DamageVuln::Vulnerable:
        return true;
    case DamageVuln::Immune:
        return false;
    case DamageVuln::AnthingExceptJumpOn:
        return damage_type != DamageType::JumpOn;
    }
    return false;
}

DamageResult TryDamageEnt(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    DamageOptions options
) {
    Ent& ent = state.ents.ents[ent_idx];

    const auto finish = [&](DamageResult result) {
        (void)options;
        return result;
    };
    if (ApplyDamageEffect(ent_idx, state, audio, damage_type, amount, false) ==
        EntDamageEffectResult::Consumed) {
        return finish(DamageResult::Hurt);
    }

    if (CanEntTakeDamageType(ent, damage_type)) {
        if (ent.stone && damage_type == DamageType::Explosion) {
            ent.health = 0;
            DieIfDead(ent_idx, state, audio);
            if (DeathWasConsumed(ent)) {
                return finish(DamageResult::Hurt);
            }
            return finish(DamageResult::Died);
        }
        bool do_damage_calculation = false;
        if (damage_type == DamageType::Crush) {
            ent.health = 0;
            (void)ApplyDamageEffect(ent_idx, state, audio, damage_type, amount, true);
            DieIfDead(ent_idx, state, audio);
            if (DeathWasConsumed(ent)) {
                return finish(DamageResult::Hurt);
            }
            if (!ent.active) {
                return finish(DamageResult::Died);
            }
            if (const std::optional<AudioAssetId> sound_effect = GetCrushAudioAssetId(ent.type_)) {
                (void)PlayEntCenterSoundEmitter(state, ent, *sound_effect);
            }
            ent.marked_for_destruction = true;
            return finish(DamageResult::Died);
        }
        if (ent.condition == EntCondition::Dead) {
            return DamageResult::None;
        } else {
            if (damage_type == DamageType::Spikes) {
                ent.health = 0;
                (void)ApplyDamageEffect(ent_idx, state, audio, damage_type, amount, true);
                DieIfDead(ent_idx, state, audio);
                if (DeathWasConsumed(ent)) {
                    return finish(DamageResult::Hurt);
                }
                return finish(DamageResult::Died);
            } else if (damage_type == DamageType::Fall) {
                do_damage_calculation = true;
                if (ent.can_be_stunned && ent.condition != EntCondition::Stunned) {
                    EnterStunnedState(ent, state);
                }
            } else if (damage_type == DamageType::Explosion) {
                do_damage_calculation = true;
                if (ent.can_be_stunned && ent.condition != EntCondition::Stunned) {
                    EnterStunnedState(ent, state);
                }
            } else if (ent.can_be_stunned) {
                if (ent.condition != EntCondition::Stunned) {
                    EnterStunnedState(ent, state);
                    do_damage_calculation = true;
                } else {
                    return DamageResult::None;
                }
            } else {
                do_damage_calculation = true;
            }
        }
        if (do_damage_calculation) {
            if (ent.health > amount) {
                ent.health -= amount;
                (void)ApplyDamageEffect(ent_idx, state, audio, damage_type, amount, true);
                return finish(DamageResult::Hurt);
            }
            ent.health = 0;
            (void)ApplyDamageEffect(ent_idx, state, audio, damage_type, amount, true);
            DieIfDead(ent_idx, state, audio);
            if (DeathWasConsumed(ent)) {
                return finish(DamageResult::Hurt);
            }
            return finish(DamageResult::Died);
        }
    }
    return DamageResult::None;
}

DamageResult TryHitEnt(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    HitOptions options
) {
    if (ent_idx >= state.ents.ents.size()) {
        return DamageResult::None;
    }

    Ent& ent = state.ents.ents[ent_idx];

    const DamageResult damage_result = TryDamageEnt(
        ent_idx,
        state,
        audio,
        damage_type,
        amount,
        DamageOptions{
            .source_vid = options.source_vid,
        }
    );

    if (damage_result == DamageResult::Hurt || damage_result == DamageResult::Died) {
        if (ent.active) {
            ApplyKnockback(ent, options.knockback);
        }
    } else if (damage_result == DamageResult::None && options.source_vid.has_value()) {
        if (options.knockback_on_no_damage && ent.active) {
            ApplyKnockback(ent, options.knockback);
        }
    }

    return damage_result;
}

} // namespace splonks::ents::common
