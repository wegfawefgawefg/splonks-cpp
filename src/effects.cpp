#include "effects.hpp"

#include "entity.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks {

namespace {

void RemoveEffectAt(Entity& entity, std::size_t effect_index) {
    if (entity.effects.get() == nullptr || effect_index >= entity.effects->count) {
        return;
    }
    for (std::size_t i = effect_index; i + 1 < entity.effects->count; ++i) {
        entity.effects->effects[i] = entity.effects->effects[i + 1];
    }
    entity.effects->count -= 1;
    entity.effects->effects[entity.effects->count] = EffectInstance{};
    if (entity.effects->count == 0) {
        entity.effects.reset();
    }
}

void ApplyModifier(float& value, const EffectModifier& modifier) {
    switch (modifier.op) {
    case EffectModifierOp::Add:
        value += modifier.value;
        return;
    case EffectModifierOp::Multiply:
        value *= modifier.value;
        return;
    case EffectModifierOp::Override:
        value = modifier.value;
        return;
    case EffectModifierOp::Min:
        value = std::min(value, modifier.value);
        return;
    case EffectModifierOp::Max:
        value = std::max(value, modifier.value);
        return;
    }
}

float GetWaterTunedModifierValue(EffectModifierTarget target, const State& state, float fallback) {
    const WaterEffectSettings& water = state.settings.water_effect;
    switch (target) {
    case EffectModifierTarget::GravityScale:
        return water.gravity_scale;
    case EffectModifierTarget::VelocityDampingX:
        return water.velocity_damping_x;
    case EffectModifierTarget::VelocityDampingY:
        return water.velocity_damping_y;
    case EffectModifierTarget::MoveSpeedScale:
        return water.move_speed_scale;
    case EffectModifierTarget::MaxFallSpeed:
        return water.max_fall_speed;
    case EffectModifierTarget::BuoyancyStrength:
        return water.buoyancy_strength;
    case EffectModifierTarget::FallTimerRate:
        return water.fall_timer_rate;
    case EffectModifierTarget::StompDamageScale:
        return water.stomp_damage_scale;
    case EffectModifierTarget::SwimImpulse:
        return water.swim_impulse;
    case EffectModifierTarget::HiddenTreasureVisibility:
    case EffectModifierTarget::JumpImpulse:
    case EffectModifierTarget::SpikeDamageTaken:
    case EffectModifierTarget::StompDamage:
    case EffectModifierTarget::StompBounceImpulse:
    case EffectModifierTarget::ThrowHorizontalBoost:
        return fallback;
    }
    return fallback;
}

EffectModifier ResolveRuntimeModifier(
    const EffectInstance& effect,
    const EffectModifier& modifier,
    const State* state
) {
    if (state == nullptr || effect.id != EffectId::InWater) {
        return modifier;
    }

    EffectModifier runtime_modifier = modifier;
    runtime_modifier.value = GetWaterTunedModifierValue(modifier.target, *state, modifier.value);
    return runtime_modifier;
}

} // namespace

BoxedEntityEffects::BoxedEntityEffects(const BoxedEntityEffects& other) {
    if (other.value != nullptr) {
        value = std::make_unique<EntityEffects>(*other.value);
    }
}

BoxedEntityEffects& BoxedEntityEffects::operator=(const BoxedEntityEffects& other) {
    if (this == &other) {
        return *this;
    }
    if (other.value != nullptr) {
        value = std::make_unique<EntityEffects>(*other.value);
        return *this;
    }
    value.reset();
    return *this;
}

EffectInstance* FindEffect(Entity& entity, EffectId id) {
    if (entity.effects.get() == nullptr) {
        return nullptr;
    }
    for (std::size_t i = 0; i < entity.effects->count; ++i) {
        if (entity.effects->effects[i].id == id) {
            return &entity.effects->effects[i];
        }
    }
    return nullptr;
}

const EffectInstance* FindEffect(const Entity& entity, EffectId id) {
    if (entity.effects.get() == nullptr) {
        return nullptr;
    }
    for (std::size_t i = 0; i < entity.effects->count; ++i) {
        if (entity.effects->effects[i].id == id) {
            return &entity.effects->effects[i];
        }
    }
    return nullptr;
}

bool HasEffect(const Entity& entity, EffectId id) {
    return FindEffect(entity, id) != nullptr;
}

EffectInstance* AddEffect(Entity& entity, EffectId id, std::int32_t count, std::uint32_t frames_remaining) {
    if (id == EffectId::None || id == EffectId::Count) {
        return nullptr;
    }

    if (EffectInstance* const existing = FindEffect(entity, id)) {
        if (count != 0) {
            existing->count += count;
        }
        if (frames_remaining != 0) {
            existing->frames_remaining = frames_remaining;
        }
        return existing;
    }

    if (entity.effects.get() == nullptr) {
        entity.effects.emplace();
    }
    if (entity.effects->count >= entity.effects->effects.size()) {
        return nullptr;
    }

    EffectInstance& effect = entity.effects->effects[entity.effects->count++];
    effect = EffectInstance{
        .id = id,
        .count = count,
        .frames_remaining = frames_remaining,
    };
    return &effect;
}

void RemoveEffect(Entity& entity, EffectId id) {
    if (entity.effects.get() == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < entity.effects->count; ++i) {
        if (entity.effects->effects[i].id == id) {
            RemoveEffectAt(entity, i);
            return;
        }
    }
}

void SetEffect(Entity& entity, EffectId id, bool enabled) {
    if (enabled) {
        (void)AddEffect(entity, id);
        return;
    }
    RemoveEffect(entity, id);
}

void StepEffectTimers(Entity& entity) {
    if (entity.effects.get() == nullptr) {
        return;
    }

    std::size_t effect_index = 0;
    while (entity.effects.get() != nullptr && effect_index < entity.effects->count) {
        EffectInstance& effect = entity.effects->effects[effect_index];
        if (effect.frames_remaining == 0) {
            ++effect_index;
            continue;
        }

        effect.frames_remaining -= 1;
        if (effect.frames_remaining == 0) {
            RemoveEffectAt(entity, effect_index);
            continue;
        }
        ++effect_index;
    }
}

float GetModifiedEffectValue(
    const Entity& entity,
    EffectModifierTarget target,
    float base_value,
    const State* state
) {
    float value = base_value;
    if (entity.effects.get() == nullptr) {
        return value;
    }
    for (std::size_t effect_index = 0; effect_index < entity.effects->count; ++effect_index) {
        const EffectInstance& effect = entity.effects->effects[effect_index];
        const EffectArchetype& archetype = GetEffectArchetype(effect.id);
        for (const EffectModifier& modifier : archetype.modifiers) {
            if (modifier.target == target) {
                ApplyModifier(value, ResolveRuntimeModifier(effect, modifier, state));
            }
        }
    }
    return value;
}

void ApplyEffectHookToEntity(Entity& entity, State& state, Audio* audio, const EffectHookContext& hook) {
    if (entity.effects.get() == nullptr) {
        return;
    }
    std::size_t effect_index = 0;
    while (entity.effects.get() != nullptr && effect_index < entity.effects->count) {
        EffectInstance& effect = entity.effects->effects[effect_index];
        const EffectArchetype& archetype = GetEffectArchetype(effect.id);
        bool expired = false;
        if (archetype.should_expire != nullptr) {
            expired = archetype.should_expire(entity, effect, state, hook);
        }
        if (!expired && archetype.on_hook != nullptr) {
            archetype.on_hook(entity, effect, state, audio, hook);
        }
        if (expired) {
            RemoveEffectAt(entity, effect_index);
            continue;
        }
        ++effect_index;
    }
}

void ApplyEffectHookToAll(State& state, Audio* audio, const EffectHookContext& hook) {
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        ApplyEffectHookToEntity(entity, state, audio, hook);
    }
}

} // namespace splonks
