#include "entities/baseball_bat.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

#include <memory>
#include <random>
#include <vector>

namespace splonks::entities::baseball_bat {

namespace {

int RandomIntInclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(generator);
}

bool AabbsIntersect(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

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

void SetEntityBaseballBat(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::BaseballBat;
    entity.size = Vec2::New(12.0F, 4.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.can_be_picked_up = false;
    entity.has_physics = false;
    entity.can_collide = false;
    entity.impassable = false;
    entity.facing = LeftOrRight::Left;
    entity.frame_data_animator.SetAnimation(frame_data_ids::BaseballBatSwing);
    entity.frame_data_animator.animate = true;
    entity.frame_data_animator.loop = false;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
}

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
        other_entity->vel = knock_back_vel;
        other_entity->acc = Vec2::New(0.0F, 0.0F);
        other_entity->thrown_by = held_by_vid;
        other_entity->thrown_immunity_timer = common::kThrownByImmunityDuration;

        const common::DamageResult damage_result = common::TryToDamageEntity(
            other_entity->vid.id, state, audio, DamageType::Attack, 1);
        switch (damage_result) {
        case common::DamageResult::Died: {
            const int random_number = RandomIntInclusive(0, 10);
            std::optional<SoundEffect> sound_effect;
            if (random_number <= 8) {
                const int another_random_number = RandomIntInclusive(0, 2);
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

void StepBaseballBat(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
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
    baseball_bat.display_state = EntityDisplayState::Neutral;
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
void StepEntityPhysicsAsBaseballBat(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

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
