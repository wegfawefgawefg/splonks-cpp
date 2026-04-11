#pragma once

#include <array>
#include "entity.hpp"

namespace splonks {

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
    DrawLayer draw_layer = DrawLayer::Middle;
    LeftOrRight facing = LeftOrRight::Left;
    EntitySuperState super_state = EntitySuperState::Idle;
    EntityState state = EntityState::Idle;
    EntityDisplayState display_state = EntityDisplayState::Neutral;
    std::uint32_t bombs = 0;
    std::uint32_t ropes = 0;
    float counter_a = 0.0F;
    DamageVulnerability damage_vulnerability = DamageVulnerability::Vulnerable;
    std::optional<EntityPassiveItem> passive_item = std::nullopt;
    EntityLabel entity_label_a = EntityLabel::None;
    Alignment alignment = Alignment::Neutral;
    FrameDataAnimator frame_data_animator{};
};

const EntityArchetype& GetEntityArchetype(EntityType type_);
void PopulateEntityArchetypesTable();
void SetEntityAs(Entity& entity, EntityType type_);
FrameDataId GetDefaultAnimationIdForArchetype(EntityType type_);

} // namespace splonks
