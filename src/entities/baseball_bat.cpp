#include "entities/baseball_bat.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <memory>
#include <vector>

namespace splonks::entities::baseball_bat {

namespace {

SwingStage GetSwingStage(const Entity& baseball_bat) {
    switch (baseball_bat.frame_data_animator.current_frame) {
    case 0:
        return SwingStage::Back;
    case 1:
        return SwingStage::Above;
    default:
        return SwingStage::Swing;
    }
}

} // namespace

extern const EntityArchetype kBaseballBatArchetype{
    .type_ = EntityType::BaseballBat,
    .size = Vec2::New(12.0F, 4.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepBaseballBat,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator{
        .animation_id = frame_data_ids::BaseballBatSwing,
        .current_frame = 0,
        .current_time = 0.0F,
        .scale = 1.0F,
        .speed = 1.0F,
        .animate = true,
        .loop = false,
        .finished = false,
    },
};

bool TryApplyBatContactToEntity(
    std::size_t bat_entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (bat_entity_idx == other_entity_idx) {
        return false;
    }
    if (bat_entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& bat_entity = state.entity_manager.entities[bat_entity_idx];
    if (!bat_entity.active || bat_entity.type_ != EntityType::BaseballBat) {
        return false;
    }
    const Entity& other_entity_const = state.entity_manager.entities[other_entity_idx];
    if (!other_entity_const.active || other_entity_const.impassable || !other_entity_const.can_collide) {
        return false;
    }
    if (bat_entity.held_by_vid.has_value() && other_entity_const.vid == *bat_entity.held_by_vid) {
        return false;
    }

    const AABB bat_aabb = common::GetContactAabbForEntity(bat_entity, graphics);
    const AABB other_aabb = common::GetContactAabbForEntity(other_entity_const, graphics);
    if (!AabbsIntersect(bat_aabb, other_aabb)) {
        return false;
    }

    const LeftOrRight bat_facing = bat_entity.facing;
    const std::optional<VID> held_by_vid = bat_entity.held_by_vid;
    const SwingStage swing_stage = GetSwingStage(bat_entity);

    if (Entity* const other_entity = state.entity_manager.GetEntityMut(other_entity_const.vid)) {
        constexpr float kKnockBackImpulse = 10.0F;
        constexpr float kAirKnockBackLift = 4.0F;
        const bool should_lift_target = !other_entity->grounded || other_entity->vel.y > 0.0F;
        Vec2 knock_back_vel = other_entity->vel;
        switch (swing_stage) {
        case SwingStage::Back:
            knock_back_vel = bat_facing == LeftOrRight::Left
                                 ? Vec2::New(kKnockBackImpulse, should_lift_target ? -kAirKnockBackLift : 0.0F)
                                 : Vec2::New(-kKnockBackImpulse, should_lift_target ? -kAirKnockBackLift : 0.0F);
            break;
        case SwingStage::Above:
            knock_back_vel = bat_facing == LeftOrRight::Left
                                 ? Vec2::New(-kKnockBackImpulse / 2.0F, -kKnockBackImpulse)
                                 : Vec2::New(kKnockBackImpulse / 2.0F, -kKnockBackImpulse);
            break;
        case SwingStage::Swing:
            knock_back_vel = bat_facing == LeftOrRight::Left
                                 ? Vec2::New(-kKnockBackImpulse, should_lift_target ? -kAirKnockBackLift : 0.0F)
                                 : Vec2::New(kKnockBackImpulse, should_lift_target ? -kAirKnockBackLift : 0.0F);
            break;
        }
        common::ApplyKnockback(
            *other_entity,
            common::KnockbackSpec{
                .velocity = knock_back_vel,
                .clear_velocity = true,
                .clear_acceleration = true,
                .thrown_by = held_by_vid,
                .thrown_immunity_timer = common::kThrownByImmunityDuration,
                .projectile_contact_damage_type = DamageType::Attack,
                .projectile_contact_damage_amount = 1,
                .projectile_contact_duration = common::kProjectileContactDuration,
            }
        );

        const common::DamageResult damage_result = common::TryDamageEntity(
            other_entity->vid.id, state, audio, DamageType::Attack, 1);
        switch (damage_result) {
        case common::DamageResult::Died: {
            const int random_number = rng::RandomIntInclusive(0, 10);
            std::optional<SoundEffect> sound_effect;
            if (random_number <= 8) {
                const int another_random_number = rng::RandomIntInclusive(0, 2);
                switch (another_random_number) {
                case 0:
                    sound_effect = SoundEffect::BaseballBatKillHit1;
                    break;
                case 1:
                    sound_effect = SoundEffect::BaseballBatKillHit2;
                    break;
                default:
                    sound_effect = SoundEffect::BaseballBatKillHit3;
                    break;
                }
            }
            if (sound_effect.has_value()) {
                audio.PlaySoundEffect(*sound_effect);
            }
            break;
        }
        case common::DamageResult::None: {
            audio.PlaySoundEffect(SoundEffect::BaseballBatMetalDink1);
            break;
        }
        case common::DamageResult::Hurt:
            audio.PlaySoundEffect(SoundEffect::Thud);
            break;
        }
        return true;
    }

    return false;
}

void StepBaseballBat(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    // delete conditions
    //  //  held by is gone // player die or seomthing, stunned, etc
    Entity& baseball_bat = state.entity_manager.entities[entity_idx];
    const std::optional<VID> held_by_vid = baseball_bat.held_by_vid;
    if (!held_by_vid.has_value()) {
        state.entity_manager.SetInactive(entity_idx);
        return;
    }
    if (baseball_bat.frame_data_animator.IsFinished()) {
        state.entity_manager.SetInactive(entity_idx);
        return;
    }

    Vec2 swinger_center = Vec2::New(0.0F, 0.0F);
    LeftOrRight swinger_facing = LeftOrRight::Left;
    if (held_by_vid.has_value()) {
        if (const Entity* const held_by = state.entity_manager.GetEntity(*held_by_vid)) {
            swinger_center = held_by->GetCenter();
            swinger_facing = held_by->facing;
        }
    }

    baseball_bat.facing = swinger_facing;
    const Vec2 mounted_center = swinger_facing == LeftOrRight::Left
                                    ? swinger_center +
                                          Vec2::New(
                                              -graphics.debug_baseball_bat_hold_offset.x,
                                              graphics.debug_baseball_bat_hold_offset.y
                                          )
                                    : swinger_center + graphics.debug_baseball_bat_hold_offset;
    baseball_bat.SetCenter(mounted_center);
    state.UpdateSidForEntity(entity_idx, graphics);
    common::TryDispatchEntityEntityOverlapContacts(
        entity_idx,
        state,
        graphics,
        audio,
        common::ContactContext{
            .phase = common::ContactPhase::SweptEntered,
            .has_impact = false,
            .mover_vid = baseball_bat.vid,
        }
    );
}

/** generalize this to all square or rectangular entities somehow */
bool IsStuff(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
    case EntityType::Box:
        return true;
    default:
        return false;
    }
}

} // namespace splonks::entities::baseball_bat
