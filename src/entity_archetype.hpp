#pragma once

#include "audio.hpp"

#include <array>
#include "entity.hpp"

namespace splonks {

struct Graphics;
struct State;

using EntityOnDeath = void (*)(std::size_t entity_idx, State& state, Audio& audio);
using EntityStepLogic =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntityStepPhysics =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);

struct EntityArchetype {
    EntityType type_ = EntityType::None;
    Vec2 size = Vec2::New(8.0F, 8.0F);
    std::uint32_t health = 0;
    bool has_physics = true;
    bool can_collide = true;
    bool can_be_picked_up = true;
    bool impassable = false;
    bool hurt_on_contact = false;
    bool crusher_pusher = false;
    bool vanish_on_death = false;
    bool can_go_on_back = false;
    bool can_hang_ledge = false;
    bool can_be_stunned = false;
    bool has_ground_friction = true;
    DrawLayer draw_layer = DrawLayer::Middle;
    LeftOrRight facing = LeftOrRight::Left;
    EntityCondition condition = EntityCondition::Normal;
    EntityAiState ai_state = EntityAiState::Idle;
    EntityState state = EntityState::Idle;
    EntityDisplayState display_state = EntityDisplayState::Neutral;
    std::uint32_t bombs = 0;
    std::uint32_t ropes = 0;
    float counter_a = 0.0F;
    DamageVulnerability damage_vulnerability = DamageVulnerability::Vulnerable;
    std::optional<EntityPassiveItem> passive_item = std::nullopt;
    std::optional<SoundEffect> death_sound_effect = std::nullopt;
    EntityOnDeath on_death = nullptr;
    EntityStepLogic step_logic = nullptr;
    EntityStepPhysics step_physics = nullptr;
    EntityLabel entity_label_a = EntityLabel::None;
    Alignment alignment = Alignment::Neutral;
    const char* debug_name = "Unknown";
    FrameDataAnimator frame_data_animator{};
};

const EntityArchetype& GetEntityArchetype(EntityType type_);
const char* GetEntityTypeName(EntityType type_);
void PopulateEntityArchetypesTable();
void SetEntityAs(Entity& entity, EntityType type_);
FrameDataId GetDefaultAnimationIdForArchetype(EntityType type_);

} // namespace splonks
