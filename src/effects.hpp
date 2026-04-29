#pragma once

#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "utils.hpp"
#include "vid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace splonks {

struct Audio;
struct Entity;
struct State;

constexpr std::size_t kMaxEntityEffects = 12;

enum class EffectId : std::uint8_t {
    None,
    Gloves,
    Spectacles,
    Compass,
    Mitt,
    SpringShoes,
    SpikeShoes,
    UdjatEye,
    Ankh,
    Meathead,
    Parachute,
    NoGravityUntilContact,
    Count,
};

enum class EffectUiKind : std::uint8_t {
    Hidden,
    Passive,
    Temporary,
};

enum class EffectModifierTarget : std::uint8_t {
    GravityScale,
    HiddenTreasureVisibility,
    JumpImpulse,
    SpikeDamageTaken,
    StompDamage,
    StompBounceImpulse,
    ThrowHorizontalBoost,
};

enum class EffectModifierOp : std::uint8_t {
    Add,
    Multiply,
    Override,
    Min,
    Max,
};

enum class EffectEventType : std::uint8_t {
    Throw,
    Death,
    Grounded,
    BlockingContact,
};

struct EffectInstance {
    EffectId id = EffectId::None;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct EntityEffects {
    std::array<EffectInstance, kMaxEntityEffects> effects{};
    std::uint8_t count = 0;
};

struct BoxedEntityEffects {
    std::unique_ptr<EntityEffects> value;

    BoxedEntityEffects() = default;
    BoxedEntityEffects(const BoxedEntityEffects& other);
    BoxedEntityEffects& operator=(const BoxedEntityEffects& other);
    BoxedEntityEffects(BoxedEntityEffects&& other) noexcept = default;
    BoxedEntityEffects& operator=(BoxedEntityEffects&& other) noexcept = default;

    EntityEffects* get() {
        return value.get();
    }

    const EntityEffects* get() const {
        return value.get();
    }

    EntityEffects& emplace() {
        value = std::make_unique<EntityEffects>();
        return *value;
    }

    void reset() {
        value.reset();
    }

    EntityEffects* operator->() {
        return value.get();
    }

    const EntityEffects* operator->() const {
        return value.get();
    }
};

struct EffectModifier {
    EffectModifierTarget target = EffectModifierTarget::GravityScale;
    EffectModifierOp op = EffectModifierOp::Add;
    float value = 0.0F;
};

struct EffectEvent {
    EffectEventType type = EffectEventType::Throw;
    std::optional<VID> actor_vid = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
};

using EffectEventHandler = void (*)(Entity& owner, EffectInstance& effect, State& state, Audio* audio, const EffectEvent& event);
using EffectExpiryPredicate = bool (*)(const Entity& owner, const EffectInstance& effect, const State& state, const EffectEvent& event);

struct EffectArchetype {
    EffectId id = EffectId::None;
    const char* debug_name = "None";
    FrameDataId icon_animation_id = kInvalidFrameDataId;
    EffectUiKind ui_kind = EffectUiKind::Hidden;
    std::array<EffectModifier, 4> modifiers{};
    std::uint8_t modifier_count = 0;
    EffectEventHandler on_event = nullptr;
    EffectExpiryPredicate should_expire = nullptr;
};

const EffectArchetype& GetEffectArchetype(EffectId id);
const char* EffectIdToString(EffectId id);
EffectInstance* FindEffect(Entity& entity, EffectId id);
const EffectInstance* FindEffect(const Entity& entity, EffectId id);
bool HasEffect(const Entity& entity, EffectId id);
EffectInstance* AddEffect(Entity& entity, EffectId id, std::int32_t count = 0, std::uint32_t frames_remaining = 0);
void RemoveEffect(Entity& entity, EffectId id);
void SetEffect(Entity& entity, EffectId id, bool enabled);
float GetModifiedEffectValue(const Entity& entity, EffectModifierTarget target, float base_value);
void DispatchEffectEventToEntity(Entity& entity, State& state, Audio* audio, const EffectEvent& event);
void DispatchEffectEventToAll(State& state, Audio* audio, const EffectEvent& event);

} // namespace splonks
