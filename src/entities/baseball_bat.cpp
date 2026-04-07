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

} // namespace

void SetEntityBaseballBat(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::BaseballBat;
    entity.size = Vec2::New(16.0F, 8.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.can_be_picked_up = false;
    entity.has_physics = false;
    entity.can_collide = false;
    entity.impassable = false;
    entity.facing = LeftOrRight::Left;
    entity.frame_data_animator.SetAnimation(frame_data_ids::BaseballBatSwing);
    entity.frame_data_animator.animate = false;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.counter_a = 0.0F;
}

void StepBaseballBat(std::size_t entity_idx, State& state, Audio& audio) {
    // delete conditions
    //  //  held by is gone // player die or seomthing, stunned, etc
    Entity& baseball_bat = state.entity_manager.entities[entity_idx];
    const std::optional<VID> held_by_vid = baseball_bat.held_by_vid;
    if (!held_by_vid.has_value()) {
        state.entity_manager.SetInactive(entity_idx);
    }
    if (baseball_bat.counter_a >= 0.0F) {
        baseball_bat.counter_a += 1.0F;
    }

    Vec2 swinger_pos = Vec2::New(0.0F, 0.0F);
    LeftOrRight swinger_facing = LeftOrRight::Left;
    if (held_by_vid.has_value()) {
        if (const Entity* const held_by = state.entity_manager.GetEntity(*held_by_vid)) {
            swinger_pos = held_by->pos;
            swinger_facing = held_by->facing;
        }
    }

    baseball_bat.pos = swinger_pos;
    baseball_bat.facing = swinger_facing;

    bool over = false;
    SwingStage swing_stage = SwingStage::Back;
    if (0.0F <= baseball_bat.counter_a && baseball_bat.counter_a <= 4.0F) {
        swing_stage = SwingStage::Back;
        baseball_bat.display_state = EntityDisplayState::Neutral;
        switch (baseball_bat.facing) {
        case LeftOrRight::Left:
            baseball_bat.pos = swinger_pos + Vec2::New(2.0F, 1.0F);
            break;
        case LeftOrRight::Right:
            baseball_bat.pos = swinger_pos + Vec2::New(-7.0F, 1.0F);
            break;
        }
    } else if (5.0F <= baseball_bat.counter_a && baseball_bat.counter_a <= 8.0F) {
        swing_stage = SwingStage::Above;
        baseball_bat.display_state = EntityDisplayState::NeutralHolding;
        switch (baseball_bat.facing) {
        case LeftOrRight::Left:
            baseball_bat.pos = swinger_pos + Vec2::New(-4.0F, -5.0F);
            break;
        case LeftOrRight::Right:
            baseball_bat.pos = swinger_pos + Vec2::New(-2.0F, -5.0F);
            break;
        }
    } else if (9.0F <= baseball_bat.counter_a && baseball_bat.counter_a <= 16.0F) {
        swing_stage = SwingStage::Swing;
        baseball_bat.display_state = EntityDisplayState::Walk;
        switch (baseball_bat.facing) {
        case LeftOrRight::Left:
            baseball_bat.pos = swinger_pos + Vec2::New(-10.0F, 2.0F);
            break;
        case LeftOrRight::Right:
            baseball_bat.pos = swinger_pos + Vec2::New(4.0F, 2.0F);
            break;
        }
    } else {
        over = true;
    }
    if (over) {
        state.entity_manager.SetInactive(entity_idx);
    }

    // HURT and/or STUN ON CONTACT
    {
        const Entity& entity = state.entity_manager.entities[entity_idx];
        const VID entity_vid = entity.vid;
        const AABB entity_aabb = entity.GetAABB();
        const LeftOrRight entity_facing = entity.facing;

        const std::vector<VID> search_results =
            state.sid.QueryExclude(entity_aabb.tl, entity_aabb.br, entity_vid);
        std::vector<VID> results;
        for (const VID& vid : search_results) {
            const Entity& e = state.entity_manager.entities[vid.id];
            if (!e.impassable) {
                results.push_back(vid);
            }
        }

        for (const VID& vid : results) {
            if (held_by_vid.has_value() && vid == *held_by_vid) {
                continue;
            }
            if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
                if (other_entity->can_collide) {
                    // TODO: target entities oof sound? or should all entities just check if their health went down and saay oof if it did
                    constexpr float kKnockBackImpulse = 10.0F;
                    Vec2 knock_back_vel = Vec2::New(0.0F, 0.0F);
                    switch (swing_stage) {
                    case SwingStage::Back:
                        knock_back_vel = entity_facing == LeftOrRight::Left
                                             ? Vec2::New(kKnockBackImpulse, 0.0F)
                                             : Vec2::New(-kKnockBackImpulse, 0.0F);
                        break;
                    case SwingStage::Above:
                        knock_back_vel = entity_facing == LeftOrRight::Left
                                             ? Vec2::New(-kKnockBackImpulse / 2.0F, -kKnockBackImpulse)
                                             : Vec2::New(kKnockBackImpulse / 2.0F, -kKnockBackImpulse);
                        break;
                    case SwingStage::Swing:
                        knock_back_vel = entity_facing == LeftOrRight::Left
                                             ? Vec2::New(-kKnockBackImpulse, 0.0F)
                                             : Vec2::New(kKnockBackImpulse, 0.0F);
                        break;
                    }
                    if (other_entity->vel.y > 0.0F) {
                        other_entity->vel.y = 0.0F;
                    }
                    other_entity->acc += knock_back_vel;
                    other_entity->thrown_by = held_by_vid;
                    other_entity->thrown_immunity_timer = common::kThrownByImmunityDuration;

                    const common::DamageResult damage_result = common::TryToDamageEntity(
                        other_entity->vid.id, state, audio, DamageType::Attack, 1);
                    switch (damage_result) {
                    case common::DamageResult::Died: {
                        // random number equal to 0, 1 or 2
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
                        // TODO: only do this if the entity isnt grounded or its annoying
                        const int random_number = RandomIntInclusive(0, 10);
                        std::optional<SoundEffect> sound_effect;
                        if (random_number <= 10) {
                            sound_effect = SoundEffect::BaseballBatMetalDink1;
                        }
                        if (sound_effect.has_value()) {
                            audio.PlaySoundEffect(*sound_effect);
                        }
                        break;
                    }
                    case common::DamageResult::Hurt:
                        audio.PlaySoundEffect(SoundEffect::Thud);
                        break;
                    }
                }
            }
        }
    }
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBaseballBat(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, audio);
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
