#include "entities/common/common.hpp"

#include "gameplay_authority.hpp"
#include "gameplay_messages.hpp"
#include "on_damage_effects.hpp"
#include "world_ops.hpp"

namespace splonks::entities::common {

namespace {

void EnterStunnedState(Entity& entity, State& state) {
    if (entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None) {
        ReleaseEntityFromHolder(entity, state);
    }
    entity.condition = EntityCondition::Stunned;
    TrySetAnimation(entity, EntityDisplayState::Stunned);
    entity.stun_timer = kDefaultStunTimer;
    DropHeldItemFromEntity(entity, state);
}

std::optional<AudioAssetId> GetCrushAudioAssetId(EntityType type_) {
    switch (type_) {
    case EntityType::Player:
        return audio_asset_ids::AnimalCrush1;
    case EntityType::Bat:
        return audio_asset_ids::AnimalCrush2;
    case EntityType::Gold:
    case EntityType::GoldStack:
    case EntityType::GoldChunk:
    case EntityType::GoldNugget:
    case EntityType::GoldBar:
    case EntityType::GoldBars:
        return audio_asset_ids::MoneySmashed;
    case EntityType::Rock:
        return audio_asset_ids::Thud;
    default:
        return std::nullopt;
    }
}

bool IsRemotePlayerEntity(const State& state, const Entity& entity) {
    const PlayerSlot* const player_slot = state.players.FindByEntityVid(entity.vid);
    return player_slot != nullptr &&
           player_slot->connection_kind == PlayerConnectionKind::Remote;
}

bool ShouldPeerRequestDamageInsteadOfApplying(
    const State& state,
    const Entity& target,
    const DamageOptions& options
) {
    if (state.net_session.role != network::NetRole::Peer) {
        return false;
    }
    if (options.source_vid.has_value()) {
        return HasLocalGameplayAuthorityForInteractionSource(state, *options.source_vid);
    }
    return HasLocalGameplayAuthorityForEntity(state, target.vid);
}

bool ShouldPeerRequestHitInsteadOfApplying(
    const State& state,
    const Entity& target,
    const HitOptions& options
) {
    if (state.net_session.role != network::NetRole::Peer) {
        return false;
    }
    if (options.source_vid.has_value()) {
        return HasLocalGameplayAuthorityForInteractionSource(state, *options.source_vid);
    }
    return HasLocalGameplayAuthorityForEntity(state, target.vid);
}

DamageResult RequestCoordinatorDamage(
    State& state,
    const Entity& target,
    DamageType damage_type,
    unsigned int amount,
    std::optional<VID> source_vid
) {
    world_ops::RequestGameplayAction(
        state,
        GameplayActionRequested{
            .kind = GameplayActionKind::DamageEntity,
            .source_vid = source_vid,
            .target_vid = target.vid,
            .world_pos = target.GetCenter(),
            .damage_type = damage_type,
            .amount = amount,
        }
    );
    return DamageResult::Requested;
}

DamageResult RequestCoordinatorHit(
    State& state,
    const Entity& target,
    DamageType damage_type,
    unsigned int amount,
    const HitOptions& options
) {
    world_ops::RequestGameplayAction(
        state,
        GameplayActionRequested{
            .kind = GameplayActionKind::HitEntity,
            .source_vid = options.source_vid,
            .target_vid = target.vid,
            .world_pos = target.GetCenter(),
            .velocity = options.knockback.velocity,
            .damage_type = damage_type,
            .projectile_contact_damage_type = options.knockback.projectile_contact_damage_type,
            .amount = amount,
            .projectile_contact_damage_amount =
                options.knockback.projectile_contact_damage_amount,
            .thrown_immunity_timer = options.knockback.thrown_immunity_timer,
            .projectile_contact_duration = options.knockback.projectile_contact_duration,
            .clear_velocity = options.knockback.clear_velocity,
            .clear_acceleration = options.knockback.clear_acceleration,
            .knockback_on_no_damage = options.knockback_on_no_damage,
        }
    );
    return DamageResult::Requested;
}

void OnDeath(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    ApplyEffectHookToAll(
        state,
        &audio,
        EffectHookContext{
            .type = EffectHookType::Death,
            .target_vid = entity.vid,
            .world_pos = entity.GetCenter(),
        }
    );
    if (entity.health > 0 && entity.condition != EntityCondition::Dead) {
        return;
    }

    const std::optional<AudioAssetId> sound_effect =
        entity.stone ? std::optional<AudioAssetId>(audio_asset_ids::PotShatter)
                     : entity.death_sound;
    if (sound_effect.has_value()) {
        (void)PlayEntityCenterSoundEmitter(state, entity, *sound_effect);
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
            (void)PlayEntityCenterSoundEmitter(state, entity, *entity.damage_sound);
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
    if (!HasLocalGameplayAuthorityForEntity(state, entity.vid)) {
        return;
    }

    DoExplosion(entity_idx, entity.GetCenter(), 2.0F, 6.0F, state, audio);
    (void)world_ops::DeactivateEntity(state, entity.vid);
}

void DieIfDead(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool entered_dead = entity.condition != EntityCondition::Dead && entity.health == 0;
    if (entity.health == 0) {
        if (entered_dead) {
            ReleaseEntityFromHolder(entity, state);
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

bool DeathWasConsumed(const Entity& entity) {
    return entity.health > 0 && entity.condition != EntityCondition::Dead;
}

bool CanEntityTakeDamageType(const Entity& entity, DamageType damage_type) {
    switch (entity.damage_vulnerability) {
    case DamageVulnerability::AttackingOnly:
        return damage_type == DamageType::Attack ||
               damage_type == DamageType::IgnitingAttack;
    case DamageVulnerability::BurningOnly:
        return damage_type == DamageType::Burn;
    case DamageVulnerability::CrushingOnly:
        return damage_type == DamageType::Crush;
    case DamageVulnerability::ExplosionOnly:
        return damage_type == DamageType::Explosion;
    case DamageVulnerability::CrushingAndSpikes:
        return damage_type == DamageType::Crush || damage_type == DamageType::Spikes;
    case DamageVulnerability::CrushingSpikesAndExplosion:
        return damage_type == DamageType::Crush || damage_type == DamageType::Spikes ||
               damage_type == DamageType::Explosion;
    case DamageVulnerability::HeavyAttackOnly:
        return damage_type == DamageType::HeavyAttack;
    case DamageVulnerability::Vulnerable:
        return true;
    case DamageVulnerability::Immune:
        return false;
    case DamageVulnerability::AnthingExceptJumpOn:
        return damage_type != DamageType::JumpOn;
    }
    return false;
}

DamageResult TryDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    DamageOptions options
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (ShouldPeerRequestDamageInsteadOfApplying(state, entity, options)) {
        return RequestCoordinatorDamage(state, entity, damage_type, amount, options.source_vid);
    }

    const bool coordinator_can_author_remote_player_target =
        state.net_session.role == network::NetRole::Coordinator &&
        options.allow_remote_player_target;
    if (!HasLocalGameplayAuthorityForEntity(state, entity.vid) &&
        !coordinator_can_author_remote_player_target) {
        if (options.source_vid.has_value() &&
            HasLocalGameplayAuthorityForInteractionSource(state, *options.source_vid)) {
            world_ops::RequestGameplayAction(
                state,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DamageEntity,
                    .source_vid = options.source_vid,
                    .target_vid = entity.vid,
                    .world_pos = entity.GetCenter(),
                    .damage_type = damage_type,
                    .amount = amount,
                }
            );
            return DamageResult::Requested;
        }
        return DamageResult::None;
    }
    if (IsRemotePlayerEntity(state, entity) && !options.allow_remote_player_target) {
        return DamageResult::None;
    }
    const auto finish = [&](DamageResult result) {
        if (!options.defer_replication &&
            (result == DamageResult::Hurt || result == DamageResult::Died)) {
            world_ops::CommitEntityDamaged(state, entity, damage_type, amount, options.source_vid);
        }
        return result;
    };
    if (ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, false) ==
        EntityDamageEffectResult::Consumed) {
        return finish(DamageResult::Hurt);
    }

    if (CanEntityTakeDamageType(entity, damage_type)) {
        if (entity.stone && damage_type == DamageType::Explosion) {
            entity.health = 0;
            DieIfDead(entity_idx, state, audio);
            if (DeathWasConsumed(entity)) {
                return finish(DamageResult::Hurt);
            }
            return finish(DamageResult::Died);
        }
        bool do_damage_calculation = false;
        if (damage_type == DamageType::Crush) {
            entity.health = 0;
            (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
            DieIfDead(entity_idx, state, audio);
            if (DeathWasConsumed(entity)) {
                return finish(DamageResult::Hurt);
            }
            if (!entity.active) {
                return finish(DamageResult::Died);
            }
            if (const std::optional<AudioAssetId> sound_effect = GetCrushAudioAssetId(entity.type_)) {
                (void)PlayEntityCenterSoundEmitter(state, entity, *sound_effect);
            }
            entity.marked_for_destruction = true;
            return finish(DamageResult::Died);
        }
        if (entity.condition == EntityCondition::Dead) {
            return DamageResult::None;
        } else {
            if (damage_type == DamageType::Spikes) {
                entity.health = 0;
                (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
                DieIfDead(entity_idx, state, audio);
                if (DeathWasConsumed(entity)) {
                    return finish(DamageResult::Hurt);
                }
                return finish(DamageResult::Died);
            } else if (damage_type == DamageType::Fall) {
                do_damage_calculation = true;
                if (entity.can_be_stunned && entity.condition != EntityCondition::Stunned) {
                    EnterStunnedState(entity, state);
                }
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
                return finish(DamageResult::Hurt);
            }
            entity.health = 0;
            (void)ApplyDamageEffect(entity_idx, state, audio, damage_type, amount, true);
            DieIfDead(entity_idx, state, audio);
            if (DeathWasConsumed(entity)) {
                return finish(DamageResult::Hurt);
            }
            return finish(DamageResult::Died);
        }
    }
    return DamageResult::None;
}

DamageResult TryHitEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    HitOptions options
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return DamageResult::None;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (ShouldPeerRequestHitInsteadOfApplying(state, entity, options)) {
        return RequestCoordinatorHit(state, entity, damage_type, amount, options);
    }

    const bool coordinator_can_author_remote_player_target =
        state.net_session.role == network::NetRole::Coordinator &&
        options.allow_remote_player_target;
    if (!HasLocalGameplayAuthorityForEntity(state, entity.vid) &&
        !coordinator_can_author_remote_player_target) {
        if (options.source_vid.has_value() &&
            HasLocalGameplayAuthorityForInteractionSource(state, *options.source_vid)) {
            world_ops::RequestGameplayAction(
                state,
                GameplayActionRequested{
                    .kind = GameplayActionKind::HitEntity,
                    .source_vid = options.source_vid,
                    .target_vid = entity.vid,
                    .world_pos = entity.GetCenter(),
                    .velocity = options.knockback.velocity,
                    .damage_type = damage_type,
                    .projectile_contact_damage_type = options.knockback.projectile_contact_damage_type,
                    .amount = amount,
                    .projectile_contact_damage_amount =
                        options.knockback.projectile_contact_damage_amount,
                    .thrown_immunity_timer = options.knockback.thrown_immunity_timer,
                    .projectile_contact_duration = options.knockback.projectile_contact_duration,
                    .clear_velocity = options.knockback.clear_velocity,
                    .clear_acceleration = options.knockback.clear_acceleration,
                    .knockback_on_no_damage = options.knockback_on_no_damage,
                }
            );
            return DamageResult::Requested;
        }
        return DamageResult::None;
    }

    const DamageResult damage_result = TryDamageEntity(
        entity_idx,
        state,
        audio,
        damage_type,
        amount,
        DamageOptions{
            .source_vid = options.source_vid,
            .allow_remote_player_target = options.allow_remote_player_target,
            .defer_replication = true,
        }
    );

    if (damage_result == DamageResult::Hurt || damage_result == DamageResult::Died) {
        if (entity.active) {
            ApplyKnockback(entity, options.knockback);
        }
        world_ops::CommitEntityDamaged(state, entity, damage_type, amount, options.source_vid);
    } else if (damage_result == DamageResult::None && options.source_vid.has_value()) {
        if (options.knockback_on_no_damage && entity.active) {
            ApplyKnockback(entity, options.knockback);
        }
        if (const Entity* const source = state.entity_manager.GetEntity(*options.source_vid)) {
            world_ops::PatchEntityState(state, *source, entity);
        }
    }

    return damage_result;
}

} // namespace splonks::entities::common
