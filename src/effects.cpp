#include "effects.hpp"

#include "ent.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks {

namespace {

void RemoveEffectAt(Ent& ent, std::size_t effect_index) {
    if (ent.effects.get() == nullptr || effect_index >= ent.effects->count) {
        return;
    }
    for (std::size_t i = effect_index; i + 1 < ent.effects->count; ++i) {
        ent.effects->effects[i] = ent.effects->effects[i + 1];
    }
    ent.effects->count -= 1;
    ent.effects->effects[ent.effects->count] = EffectInstance{};
    if (ent.effects->count == 0) {
        ent.effects.reset();
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

BoxedEntEffects::BoxedEntEffects(const BoxedEntEffects& other) {
    if (other.value != nullptr) {
        value = std::make_unique<EntEffects>(*other.value);
    }
}

BoxedEntEffects& BoxedEntEffects::operator=(const BoxedEntEffects& other) {
    if (this == &other) {
        return *this;
    }
    if (other.value != nullptr) {
        value = std::make_unique<EntEffects>(*other.value);
        return *this;
    }
    value.reset();
    return *this;
}

EffectInstance* FindEffect(Ent& ent, EffectId id) {
    if (ent.effects.get() == nullptr) {
        return nullptr;
    }
    for (std::size_t i = 0; i < ent.effects->count; ++i) {
        if (ent.effects->effects[i].id == id) {
            return &ent.effects->effects[i];
        }
    }
    return nullptr;
}

const EffectInstance* FindEffect(const Ent& ent, EffectId id) {
    if (ent.effects.get() == nullptr) {
        return nullptr;
    }
    for (std::size_t i = 0; i < ent.effects->count; ++i) {
        if (ent.effects->effects[i].id == id) {
            return &ent.effects->effects[i];
        }
    }
    return nullptr;
}

bool HasEffect(const Ent& ent, EffectId id) {
    return FindEffect(ent, id) != nullptr;
}

EffectInstance* AddEffect(Ent& ent, EffectId id, std::int32_t count, std::uint32_t frames_remaining) {
    if (id == EffectId::None || id == EffectId::Count) {
        return nullptr;
    }

    if (EffectInstance* const existing = FindEffect(ent, id)) {
        if (count != 0) {
            existing->count += count;
        }
        if (frames_remaining != 0) {
            existing->frames_remaining = frames_remaining;
        }
        return existing;
    }

    if (ent.effects.get() == nullptr) {
        ent.effects.emplace();
    }
    if (ent.effects->count >= ent.effects->effects.size()) {
        return nullptr;
    }

    EffectInstance& effect = ent.effects->effects[ent.effects->count++];
    effect = EffectInstance{
        .id = id,
        .count = count,
        .frames_remaining = frames_remaining,
    };
    return &effect;
}

void RemoveEffect(Ent& ent, EffectId id) {
    if (ent.effects.get() == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < ent.effects->count; ++i) {
        if (ent.effects->effects[i].id == id) {
            RemoveEffectAt(ent, i);
            return;
        }
    }
}

void SetEffect(Ent& ent, EffectId id, bool enabled) {
    if (enabled) {
        (void)AddEffect(ent, id);
        return;
    }
    RemoveEffect(ent, id);
}

void StepEffectTimers(Ent& ent) {
    if (ent.effects.get() == nullptr) {
        return;
    }

    std::size_t effect_index = 0;
    while (ent.effects.get() != nullptr && effect_index < ent.effects->count) {
        EffectInstance& effect = ent.effects->effects[effect_index];
        if (effect.frames_remaining == 0) {
            ++effect_index;
            continue;
        }

        effect.frames_remaining -= 1;
        if (effect.frames_remaining == 0) {
            RemoveEffectAt(ent, effect_index);
            continue;
        }
        ++effect_index;
    }
}

float GetModifiedEffectValue(
    const Ent& ent,
    EffectModifierTarget target,
    float base_value,
    const State* state
) {
    float value = base_value;
    if (ent.effects.get() == nullptr) {
        return value;
    }
    for (std::size_t effect_index = 0; effect_index < ent.effects->count; ++effect_index) {
        const EffectInstance& effect = ent.effects->effects[effect_index];
        const EffectSpec& spec = GetEffectSpec(effect.id);
        for (const EffectModifier& modifier : spec.modifiers) {
            if (modifier.target == target) {
                ApplyModifier(value, ResolveRuntimeModifier(effect, modifier, state));
            }
        }
    }
    return value;
}

void ApplyEffectHookToEnt(Ent& ent, State& state, Audio* audio, const EffectHookContext& hook) {
    if (ent.effects.get() == nullptr) {
        return;
    }
    std::size_t effect_index = 0;
    while (ent.effects.get() != nullptr && effect_index < ent.effects->count) {
        EffectInstance& effect = ent.effects->effects[effect_index];
        const EffectSpec& spec = GetEffectSpec(effect.id);
        bool expired = false;
        if (spec.should_expire != nullptr) {
            expired = spec.should_expire(ent, effect, state, hook);
        }
        if (!expired && spec.on_hook != nullptr) {
            spec.on_hook(ent, effect, state, audio, hook);
        }
        if (expired) {
            RemoveEffectAt(ent, effect_index);
            continue;
        }
        ++effect_index;
    }
}

void ApplyEffectHookToAll(State& state, Audio* audio, const EffectHookContext& hook) {
    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        ApplyEffectHookToEnt(ent, state, audio, hook);
    }
}

} // namespace splonks
