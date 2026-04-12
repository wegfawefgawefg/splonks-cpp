#include "entities/common.hpp"

#include "entities/player.hpp"

#include <algorithm>

namespace splonks::entities::common {

namespace {

constexpr float kStompHeadHeight = 1.0F;

} // namespace

bool TryApplyStompContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& stomper = state.entity_manager.entities[entity_idx];
    Entity* const stomped = state.entity_manager.GetEntityMut(state.entity_manager.entities[other_entity_idx].vid);
    if (!stomper.active || stomped == nullptr || !stomped->active) {
        return false;
    }

    if (stomper.vel.y <= 0.0F) {
        return false;
    }
    if (stomped->impassable || !stomped->can_collide ||
        stomped->condition == EntityCondition::Dead ||
        stomped->condition == EntityCondition::Stunned ||
        stomped->alignment != Alignment::Enemy) {
        return false;
    }

    const AABB stomper_aabb = stomper.GetAABB();
    const AABB stomped_aabb = stomped->GetAABB();
    const float stomped_head_band_bottom = stomped_aabb.tl.y + kStompHeadHeight;
    if (stomper_aabb.br.y > stomped_head_band_bottom) {
        return false;
    }

    audio.PlaySoundEffect(SoundEffect::Jump);

    const DamageResult damage_result = TryToDamageEntity(
        stomped->vid.id,
        state,
        audio,
        DamageType::JumpOn,
        1
    );
    if (stomped->can_be_stunned) {
        stomped->condition = EntityCondition::Stunned;
        stomped->stun_timer = kDefaultStunTimer;
    }
    if (damage_result == DamageResult::Died || damage_result == DamageResult::Hurt) {
        stomped->thrown_by = stomper.vid;
        stomped->thrown_immunity_timer = kThrownByImmunityDuration;
    }
    state.AddInteractionCooldown(
        stomped->vid,
        stomper.vid,
        InteractionCooldownKind::Harm,
        1
    );

    stomper.vel.y = -player::kJumpImpulse;
    return true;
}

} // namespace splonks::entities::common
