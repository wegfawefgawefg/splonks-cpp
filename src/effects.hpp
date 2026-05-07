#pragma once

#include "effects/effect_id.hpp"
#include "frame_data_id.hpp"
#include "hud/types.hpp"
#include "math_types.hpp"
#include "utils.hpp"
#include "vid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SDL_Renderer;

namespace splonks {

struct Audio;
struct Entity;
struct Graphics;
struct State;

constexpr std::size_t kMaxEntityEffects = 12;

enum class EffectUiKind : std::uint8_t {
    Hidden,
    Passive,
    Temporary,
};

enum class EffectModifierTarget : std::uint8_t {
    // Multiplies gravity acceleration. Base: 1.0.
    GravityScale,
    // Multiplies velocity after physics integration. Base: 1.0.
    VelocityDampingX,
    VelocityDampingY,
    // Multiplies controller-chosen horizontal top speeds. Base: 1.0.
    MoveSpeedScale,
    // Clamps positive/downward velocity. Base: entity/controller max fall speed.
    MaxFallSpeed,
    // Upward acceleration applied from fluid-like effects, scaled by entity buoyancy. Base: 0.0.
    BuoyancyStrength,
    // Multiplies fall danger accumulation. Override 0 disables fall damage buildup. Base: 1.0.
    FallTimerRate,
    HiddenTreasureVisibility,
    JumpImpulse,
    SpikeDamageTaken,
    StompDamage,
    // Multiplies final stomp damage. Override 0 disables stomp attempts. Base: 1.0.
    StompDamageScale,
    StompBounceImpulse,
    ThrowHorizontalBoost,
    // Enables a controller/common helper to swim upward on jump. Base: 0.0.
    SwimImpulse,
};

enum class EffectModifierOp : std::uint8_t {
    Add,
    Multiply,
    Override,
    Min,
    Max,
};

enum class EffectHookType : std::uint8_t {
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

struct EffectHookContext {
    EffectHookType type = EffectHookType::Throw;
    std::optional<VID> actor_vid = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
};

using EffectHookHandler = void (*)(Entity& owner, EffectInstance& effect, State& state, Audio* audio, const EffectHookContext& hook);
using EffectExpiryPredicate = bool (*)(const Entity& owner, const EffectInstance& effect, const State& state, const EffectHookContext& hook);
using EffectHudCountTextFn = std::optional<std::string> (*)(const EffectInstance& effect);
using EffectWorldOverlayFn = void (*)(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Entity& owner,
    const EffectInstance& effect
);

struct EffectArchetype {
    EffectId id = EffectId::None;
    const char* debug_name = "None";
    FrameDataId icon_animation_id = kInvalidFrameDataId;
    EffectUiKind ui_kind = EffectUiKind::Hidden;
    std::int32_t default_count = 0;
    EffectHudCountTextFn hud_count_text = nullptr;
    HudAnchor hud_count_anchor = HudAnchor::BottomRight;
    EffectWorldOverlayFn render_world_overlay = nullptr;
    std::vector<EffectModifier> modifiers;
    EffectHookHandler on_hook = nullptr;
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
void StepEffectTimers(Entity& entity);
float GetModifiedEffectValue(
    const Entity& entity,
    EffectModifierTarget target,
    float base_value,
    const State* state = nullptr
);
void ApplyEffectHookToEntity(Entity& entity, State& state, Audio* audio, const EffectHookContext& hook);
void ApplyEffectHookToAll(State& state, Audio* audio, const EffectHookContext& hook);

} // namespace splonks
