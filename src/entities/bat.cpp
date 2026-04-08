#include "entities/bat.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <algorithm>
#include <random>
#include <vector>

namespace splonks::entities::bat {

namespace {

Vec2 NormalizeOrZero(const Vec2& value) {
    const float length = Length(value);
    if (length == 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }
    return value / length;
}

bool RandomBool() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(0, 1);
    return distribution(generator) == 0;
}

IAABB GetAreaAbove(const Entity& bat) {
    const auto [tl, br] = bat.GetBounds();
    return IAABB{
        .tl = IVec2::New(static_cast<int>(tl.x), static_cast<int>(tl.y) - 1),
        .br = IVec2::New(static_cast<int>(br.x), static_cast<int>(tl.y)),
    };
}

bool IsAtPerchOrRoof(const Entity& bat, const State& state) {
    const IAABB area_above = GetAreaAbove(bat);
    const std::vector<const Tile*> tiles_above_you =
        state.stage.GetTilesInRectWc(area_above.tl, area_above.br);
    return CollidableTileInList(tiles_above_you);
}

void SnapBatToRoof(Entity& bat, const State& state) {
    if (IsAtPerchOrRoof(bat, state)) {
        return;
    }

    for (int i = 0; i < static_cast<int>(kTileSize); ++i) {
        bat.pos.y -= 1.0F;
        if (IsAtPerchOrRoof(bat, state)) {
            return;
        }
    }
}

} // namespace

void SetEntityBat(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::Bat;
    entity.size = Vec2::New(8.0F, 8.0F);
    entity.health = 1;
    entity.hurt_on_contact = true;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.entity_a.reset();
    entity.entity_label_a = EntityLabel::AttackThis;
    entity.impassable = false;
    entity.frame_data_animator.SetAnimation(frame_data_ids::HangingBat);
    entity.can_be_stunned = true;
    entity.alignment = Alignment::Enemy;
}

/** Bat goes up by default, and idles if it hits the ceiling.
 *  If the bat detects the player is beneath it,
 *  It checks if the player is within some dist below, some dist left or right.
 *      if yes, move towards the player right now.
 *  If no, give up and fly back to the ceiling.
 */
void StepEntityLogicAsBat(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& bat = state.entity_manager.entities[entity_idx];
    if (bat.super_state != EntitySuperState::Dead) {
        constexpr int kVerticalDetectDist = 8 * static_cast<int>(kTileSize);
        constexpr int kHorizontalChaseDist = 4 * static_cast<int>(kTileSize);

        // Check for player, acquire his position
        std::optional<Vec2> target_position;
        {
            const Vec2 bat_pos = state.entity_manager.entities[entity_idx].pos;
            if (state.player_vid.has_value()) {
                if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
                    if (player->pos.y > bat_pos.y &&
                        std::abs(player->pos.y - bat_pos.y) < static_cast<float>(kVerticalDetectDist) &&
                        std::abs(player->pos.x - bat_pos.x) < static_cast<float>(kHorizontalChaseDist) &&
                        player->super_state != EntitySuperState::Stunned &&
                        player->super_state != EntitySuperState::Dead) {
                        target_position = player->pos;
                    }
                }
            }
        }

        //  State Machine
        Entity& mutable_bat = state.entity_manager.entities[entity_idx];
        if (target_position.has_value()) {
            //  Chase The Player
            //  is this the begining of a pursuit?
            if (mutable_bat.super_state == EntitySuperState::Idle) {
                //  squeak
                const SoundEffect sound_effect =
                    RandomBool() ? SoundEffect::BatSqueak : SoundEffect::BatFlap1;
                audio.PlaySoundEffect(sound_effect);
            }
            //  go to the target
            mutable_bat.super_state = EntitySuperState::Pursuing;
            mutable_bat.acc += NormalizeOrZero(*target_position - mutable_bat.pos) * kChaseSpeed;
            mutable_bat.display_state = EntityDisplayState::Fly;
        } else {
            //  Go Back To Your Perch, (straight up from here lol)
            mutable_bat.super_state = EntitySuperState::Returning;
            //  did you arrive at the perch
            const bool at_perch_or_roof = IsAtPerchOrRoof(mutable_bat, state);
            if (at_perch_or_roof) {
                mutable_bat.super_state = EntitySuperState::Idle;
                mutable_bat.acc = Vec2::New(0.0F, 0.0F);
                mutable_bat.vel = Vec2::New(0.0F, 0.0F);
                mutable_bat.display_state = EntityDisplayState::Neutral;
            } else {
                //  keep going up till you get there
                mutable_bat.acc += Vec2::New(0.0F, -2.0F);
                mutable_bat.vel.x = 0.0F;
                mutable_bat.display_state = EntityDisplayState::Fly;
            }
        }
        if (mutable_bat.vel.x < 0.0F) {
            mutable_bat.facing = LeftOrRight::Left;
        }

        if (mutable_bat.vel.x > 0.0F) {
            mutable_bat.facing = LeftOrRight::Right;
        }
    }
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBat(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    Entity& bat = state.entity_manager.entities[entity_idx];
    const EntitySuperState bat_super_state = bat.super_state;
    const bool entered_idle_this_frame =
        bat_super_state == EntitySuperState::Idle && bat.last_super_state != EntitySuperState::Idle;

    if (entered_idle_this_frame) {
        SnapBatToRoof(bat, state);
    }

    if (bat_super_state == EntitySuperState::Idle) {
        bat.acc = Vec2::New(0.0F, 0.0F);
        bat.vel = Vec2::New(0.0F, 0.0F);
    } else {
        common::ApplyGravity(entity_idx, state, dt);
    }

    common::PrePartialEulerStep(entity_idx, state, dt);
    if (bat.super_state == EntitySuperState::Pursuing ||
        bat.super_state == EntitySuperState::Returning) {
        bat.vel.x = std::clamp(bat.vel.x, -kChaseMaxSpeed, kChaseMaxSpeed);
        bat.vel.y = std::clamp(bat.vel.y, -kChaseMaxSpeed, kChaseMaxSpeed);
    } else if (bat.super_state == EntitySuperState::Dead ||
               bat.super_state == EntitySuperState::Stunned) {
        common::ApplyGroundFriction(entity_idx, state);
    }
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    if (bat_super_state != EntitySuperState::Pursuing) {
    }
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::bat
