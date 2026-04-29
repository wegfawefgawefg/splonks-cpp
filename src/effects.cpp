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

float GetModifiedEffectValue(const Entity& entity, EffectModifierTarget target, float base_value) {
    float value = base_value;
    if (entity.effects.get() == nullptr) {
        return value;
    }
    for (std::size_t effect_index = 0; effect_index < entity.effects->count; ++effect_index) {
        const EffectArchetype& archetype = GetEffectArchetype(entity.effects->effects[effect_index].id);
        for (std::uint8_t modifier_index = 0; modifier_index < archetype.modifier_count; ++modifier_index) {
            const EffectModifier& modifier = archetype.modifiers[modifier_index];
            if (modifier.target == target) {
                ApplyModifier(value, modifier);
            }
        }
    }
    return value;
}

void DispatchEffectEventToEntity(Entity& entity, State& state, Audio* audio, const EffectEvent& event) {
    if (entity.effects.get() == nullptr) {
        return;
    }
    std::size_t effect_index = 0;
    while (entity.effects.get() != nullptr && effect_index < entity.effects->count) {
        EffectInstance& effect = entity.effects->effects[effect_index];
        const EffectArchetype& archetype = GetEffectArchetype(effect.id);
        bool expired = false;
        if (archetype.should_expire != nullptr) {
            expired = archetype.should_expire(entity, effect, state, event);
        }
        if (!expired && archetype.on_event != nullptr) {
            archetype.on_event(entity, effect, state, audio, event);
        }
        if (expired) {
            RemoveEffectAt(entity, effect_index);
            continue;
        }
        ++effect_index;
    }
}

void DispatchEffectEventToAll(State& state, Audio* audio, const EffectEvent& event) {
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        DispatchEffectEventToEntity(entity, state, audio, event);
    }
}

} // namespace splonks
