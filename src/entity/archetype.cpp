#include "entity/archetype.hpp"

#include "frame_data_id.hpp"

namespace splonks {

void SetEntityAs(Entity& entity, EntityType type_) {
    const VID existing_vid = entity.vid;
    entity.Reset();
    const EntityArchetype& archetype = GetEntityArchetype(type_);
    entity.type_ = archetype.type_;
    entity.has_physics = archetype.has_physics;
    entity.can_collide = archetype.can_collide;
    entity.can_be_picked_up = archetype.can_be_picked_up;
    entity.impassable = archetype.impassable;
    entity.hurt_on_contact = archetype.hurt_on_contact;
    entity.vanish_on_death = archetype.vanish_on_death;
    entity.has_ground_friction = archetype.has_ground_friction;
    entity.passive_item = archetype.passive_item;
    entity.damage_animation = archetype.damage_animation;
    entity.damage_sound = archetype.damage_sound;
    entity.collide_sound = archetype.collide_sound;
    entity.death_sound_effect = archetype.death_sound_effect;
    entity.on_death = archetype.on_death;
    entity.on_damage = archetype.on_damage;
    entity.on_use = archetype.on_use;
    entity.step_logic = archetype.step_logic;
    entity.step_physics = archetype.step_physics;
    entity.crusher_pusher = archetype.crusher_pusher;
    entity.can_go_on_back = archetype.can_go_on_back;
    entity.can_hang_ledge = archetype.can_hang_ledge;
    entity.can_be_stunned = archetype.can_be_stunned;
    entity.stun_recovers_on_ground = archetype.stun_recovers_on_ground;
    entity.stun_recovers_while_held = archetype.stun_recovers_while_held;
    entity.size = archetype.size;
    entity.facing = archetype.facing;
    entity.draw_layer = archetype.draw_layer;
    TrySetAnimation(entity, archetype.display_state);
    entity.condition = archetype.condition;
    entity.ai_state = archetype.ai_state;
    entity.health = archetype.health;
    entity.bombs = archetype.bombs;
    entity.ropes = archetype.ropes;
    entity.counter_a = archetype.counter_a;
    entity.counter_b = archetype.counter_b;
    entity.damage_vulnerability = archetype.damage_vulnerability;
    entity.entity_label_a = archetype.entity_label_a;
    entity.alignment = archetype.alignment;
    entity.frame_data_animator = archetype.frame_data_animator;
    entity.vid = existing_vid;
}

FrameDataId GetDefaultAnimationIdForArchetype(EntityType type_) {
    const FrameDataId animation_id = GetEntityArchetype(type_).frame_data_animator.animation_id;
    if (animation_id == kInvalidFrameDataId) {
        return frame_data_ids::NoSprite;
    }
    return animation_id;
}

} // namespace splonks
