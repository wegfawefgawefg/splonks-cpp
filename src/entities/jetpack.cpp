#include "entities/jetpack.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "special_effects/ultra_dynamic_effect.hpp"
#include "state.hpp"

#include <memory>
#include <random>

namespace splonks::entities::jetpack {

namespace {

int RandomIntExclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(generator);
}

float RandomFloat(float minimum, float maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(generator);
}

} // namespace

extern const EntityArchetype kJetPackArchetype{
    .type_ = EntityType::JetPack,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = kFuel,
    .damage_vulnerability = DamageVulnerability::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsJetpack,
    .step_logic = StepEntityLogicAsJetpack,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Jetpack),
};

void OnDeathAsJetpack(std::size_t entity_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(entity_idx, state, audio);
}

/** jetpack goes up by default, and idles if it hits the ceiling.
 *  If the jetpack detects the player is beneath it,
 *  It checks if the player is within some dist below, some dist left or right.
 *      if yes, move towards the player right now.
 *  If no, give up and fly back to the ceiling.
 */
void StepEntityLogicAsJetpack(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& jetpack = state.entity_manager.entities[entity_idx];
    if (jetpack.attachment_mode == AttachmentMode::Back) {
        FrameDataId equipped_animation = frame_data_ids::JetpackBack;
        if (jetpack.held_by_vid.has_value()) {
            if (const Entity* const holder = state.entity_manager.GetEntity(*jetpack.held_by_vid)) {
                if (holder->IsHanging()) {
                    equipped_animation = frame_data_ids::JetpackSide;
                } else if (holder->climbing) {
                    equipped_animation = frame_data_ids::JetpackBack;
                }
            }
        }
        SetAnimation(jetpack, equipped_animation);
    } else if (jetpack.held_by_vid.has_value()) {
        SetAnimation(jetpack, frame_data_ids::JetpackSide);
    } else {
        SetAnimation(jetpack, frame_data_ids::Jetpack);
    }

    // if jetpack is in use, add acc to player
    const EntityState jetpack_state = jetpack.state;
    const std::optional<VID> held_by_vid = jetpack.held_by_vid;
    const float fuel = jetpack.counter_a;
    if (jetpack_state == EntityState::InUse) {
        bool refill_fuel = false;
        if (held_by_vid.has_value()) {
            if (Entity* const holder = state.entity_manager.GetEntityMut(*held_by_vid)) {
                if (holder->grounded || holder->climbing || holder->IsHanging() ||
                    holder->jumped_this_frame) {
                    refill_fuel = true;
                }
            }
        }
        if (refill_fuel) {
            jetpack.counter_a = kFuel;
        }
        if (fuel > 0.0F) {
            // make the holder go up
            if (held_by_vid.has_value()) {
                if (Entity* const holder = state.entity_manager.GetEntityMut(*held_by_vid)) {
                    const float jetpack_max_upspeed = -2.0F;
                    if (holder->vel.y > jetpack_max_upspeed) {
                        holder->acc.y = -0.6F;
                        holder->vel.y = Min(holder->vel.y, jetpack_max_upspeed);
                    }
                    TrySetAnimation(*holder, EntityDisplayState::Neutral);
                }
            }

            // play travel sound
            jetpack.travel_sound_countdown -= 1.0F;

            if (jetpack.travel_sound_countdown < 0.0F) {
                jetpack.travel_sound_countdown = kTravelSoundDistInterval;
                const SoundEffect sound_effect =
                    jetpack.travel_sound == TravelSound::One ? SoundEffect::Jetpack1
                                                             : SoundEffect::Jetpack2;
                audio.PlaySoundEffect(sound_effect);
                jetpack.IncTravelSound();
            }
            if (fuel > 0.0F) {
                jetpack.counter_a -= 1.0F;
            }

            const Vec2 center = jetpack.GetCenter();
            for (int i = 0; i < 16; ++i) {
                const float vel = RandomFloat(0.1F, 0.5F);
                const float svel = RandomFloat(vel * 0.1F, vel * 1.0F);
                const float sacc = RandomFloat(vel * 0.01F, vel * 0.02F);
                auto effect = std::make_unique<UltraDynamicEffect>();
                effect->type_ = SpecialEffectType::BasicSmoke;
                effect->draw_layer = DrawLayer::Foreground;
                effect->counter = static_cast<std::uint32_t>(RandomIntExclusive(0, 32));
                effect->pos = center + Vec2::New(3.0F, 3.0F);
                effect->size = Vec2::New(1.0F, 1.0F) * 2.0F;
                effect->rot = RandomFloat(0.0F, 360.0F);
                effect->alpha = 1.0F;
                effect->vel = Vec2::New(0.0F, RandomFloat(0.0F, 0.3F));
                effect->svel = Vec2::New(svel, svel);
                effect->rotvel = RandomFloat(-0.2F, -0.01F);
                effect->alpha_vel = vel * 0.001F;
                effect->acc = Vec2::New(0.0F, 0.0F);
                effect->sacc = Vec2::New(sacc, sacc);
                effect->rotacc = 0.0F;
                effect->alpha_acc = 0.0F;
                state.special_effects.push_back(std::move(effect));
            }
            for (int i = 0; i < 16; ++i) {
                const float vel = RandomFloat(0.1F, 0.5F);
                const float svel = RandomFloat(vel * 0.1F, vel * 1.0F);
                const float sacc = RandomFloat(vel * 0.01F, vel * 0.02F);
                auto effect = std::make_unique<UltraDynamicEffect>();
                effect->type_ = SpecialEffectType::BasicSmoke;
                effect->draw_layer = DrawLayer::Foreground;
                effect->counter = static_cast<std::uint32_t>(RandomIntExclusive(0, 32));
                effect->pos = center + Vec2::New(-3.0F, 3.0F);
                effect->size = Vec2::New(1.0F, 1.0F) * 2.0F;
                effect->rot = RandomFloat(0.0F, 360.0F);
                effect->alpha = 1.0F;
                effect->vel = Vec2::New(0.0F, RandomFloat(0.0F, 0.3F));
                effect->svel = Vec2::New(svel, svel);
                effect->rotvel = RandomFloat(-0.2F, -0.01F);
                effect->alpha_vel = vel * 0.001F;
                effect->acc = Vec2::New(0.0F, 0.0F);
                effect->sacc = Vec2::New(sacc, sacc);
                effect->rotacc = 0.0F;
                effect->alpha_acc = 0.0F;
                state.special_effects.push_back(std::move(effect));
            }
        }
    } else if (jetpack_state == EntityState::Idle) {
        jetpack.travel_sound_countdown = 0.0F;
    }
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::jetpack
