#include "entities/common.hpp"

#include "entity_display_states.hpp"
#include "entities/player.hpp"
#include "on_damage_effects.hpp"
#include "tile.hpp"

#include <algorithm>
#include <array>
#include <compare>
#include <cmath>
#include <vector>

namespace splonks::entities::common {

namespace {

const FrameData* GetCurrentFrameDataForEntity(const Entity& entity, const Graphics& graphics) {
    if (!entity.frame_data_animator.HasAnimation()) {
        return nullptr;
    }

    const FrameDataAnimation* const animation =
        graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return nullptr;
    }

    std::size_t frame_index = entity.frame_data_animator.current_frame;
    if (frame_index >= animation->frame_indices.size()) {
        frame_index = 0;
    }

    return &graphics.frame_data_db.frames[animation->frame_indices[frame_index]];
}

void ApplyFrameDataGeometryToEntity(std::size_t entity_idx, State& state, const Graphics& graphics) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const FrameData* const frame_data = GetCurrentFrameDataForEntity(entity, graphics);
    if (frame_data == nullptr) {
        return;
    }

    if (frame_data->cbox.w <= 0 || frame_data->cbox.h <= 0) {
        return;
    }

    entity.size = Vec2::New(
        static_cast<float>(frame_data->cbox.w),
        static_cast<float>(frame_data->cbox.h)
    );
}

bool MaybeHurtAndStunOnContactAsProjectile(std::size_t entity_idx, State& state, Audio& audio);

AABB GetAabbAtPosition(const Entity& entity, const Vec2& pos) {
    return AABB::New(pos, pos + entity.size - Vec2::New(1.0F, 1.0F));
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

bool IsBlockedByStageBounds(const AABB& aabb, const Stage& stage) {
    if (aabb.tl.x < 0.0F || aabb.tl.y < 0.0F) {
        return true;
    }
    if (aabb.br.x > static_cast<float>(stage.GetWidth() - 1)) {
        return true;
    }
    if (aabb.br.y > static_cast<float>(stage.GetHeight() - 1)) {
        return true;
    }
    return false;
}

bool IsBlockedByTiles(const AABB& aabb, const Stage& stage) {
    const std::vector<const Tile*> collided_tiles =
        stage.GetTilesInRectWc(ToIVec2(aabb.tl), ToIVec2(aabb.br));
    return CollidableTileInList(collided_tiles);
}

bool IsBlockedByImpassableEntities(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state
) {
    const VID self_vid = state.entity_manager.entities[entity_idx].vid;
    for (const Entity& other : state.entity_manager.entities) {
        if (!other.active) {
            continue;
        }
        if (other.vid.id == self_vid.id) {
            continue;
        }
        if (!other.impassable) {
            continue;
        }
        if (AabbsIntersect(aabb, other.GetAABB())) {
            return true;
        }
    }
    return false;
}

bool IsBlockingAabbForEntity(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state,
    bool check_tiles,
    bool check_entities
) {
    if (check_tiles) {
        if (IsBlockedByStageBounds(aabb, state.stage)) {
            return true;
        }
        if (IsBlockedByTiles(aabb, state.stage)) {
            return true;
        }
    }

    if (check_entities && IsBlockedByImpassableEntities(entity_idx, aabb, state)) {
        return true;
    }

    return false;
}

int GetIntegerStepDistance(float distance, unsigned int time) {
    const float abs_distance = std::abs(distance);
    int integer_distance = static_cast<int>(std::floor(abs_distance));
    const float fractional_distance = abs_distance - static_cast<float>(integer_distance);
    if (fractional_distance != 0.0F) {
        const int fractional_period = static_cast<int>(std::round(1.0F / fractional_distance));
        if (fractional_period != 0 && (time % static_cast<unsigned int>(fractional_period)) == 0U) {
            integer_distance += 1;
        }
    }
    if (distance < 0.0F) {
        integer_distance *= -1;
    }
    return integer_distance;
}

void StoreDistanceTraveled(std::size_t entity_idx, State& state, const Vec2& start_pos) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    float dist_traveled = Length(entity.pos - start_pos);
    if (dist_traveled < 1.0F) {
        dist_traveled = 0.0F;
    }
    entity.dist_traveled_this_frame = dist_traveled;
}

void ResolveBlockingOverlap(
    std::size_t entity_idx,
    State& state,
    bool check_tiles,
    bool check_entities
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const AABB current_aabb = entity.GetAABB();
    if (!IsBlockingAabbForEntity(entity_idx, current_aabb, state, check_tiles, check_entities)) {
        return;
    }

    const int max_push = static_cast<int>(kTileSize) * 2;
    const std::array<IVec2, 4> candidates = {
        IVec2::New(0, -1),
        IVec2::New(-1, 0),
        IVec2::New(1, 0),
        IVec2::New(0, 1),
    };

    for (int distance = 1; distance <= max_push; ++distance) {
        for (const IVec2& direction : candidates) {
            const Vec2 candidate_pos = entity.pos + ToVec2(direction * distance);
            const AABB candidate_aabb = GetAabbAtPosition(entity, candidate_pos);
            if (!IsBlockingAabbForEntity(
                    entity_idx,
                    candidate_aabb,
                    state,
                    check_tiles,
                    check_entities
                )) {
                entity.pos = candidate_pos;
                return;
            }
        }
    }
}

void MoveEntityPixelStep(
    std::size_t entity_idx,
    State& state,
    bool check_tiles,
    bool check_entities
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const Vec2 start_pos = entity.pos;

    ResolveBlockingOverlap(entity_idx, state, check_tiles, check_entities);

    const int move_x = GetIntegerStepDistance(entity.vel.x, state.stage_frame);
    const int move_y = GetIntegerStepDistance(entity.vel.y, state.stage_frame);

    if (move_x > 0) {
        for (int i = 0; i < move_x; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(1.0F, 0.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
        }
    } else if (move_x < 0) {
        for (int i = 0; i < -move_x; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(-1.0F, 0.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.x = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
        }
    }

    if (move_y > 0) {
        for (int i = 0; i < move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, 1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
        }
    } else if (move_y < 0) {
        for (int i = 0; i < -move_y; ++i) {
            const Vec2 next_pos = entity.pos + Vec2::New(0.0F, -1.0F);
            const AABB next_aabb = GetAabbAtPosition(entity, next_pos);
            if (IsBlockingAabbForEntity(entity_idx, next_aabb, state, check_tiles, check_entities)) {
                entity.vel.y = 0.0F;
                entity.collided = true;
                break;
            }
            entity.pos = next_pos;
        }
    }

    StoreDistanceTraveled(entity_idx, state, start_pos);
}

void CrushIfCanBeCrushed(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Crushed) {
        entity.health = 0;
        entity.marked_for_destruction = true;
        // TODO, fill with the other entities
        // subdivide into categories of sound to reduce the burden here
        std::optional<SoundEffect> sound_effect;
        switch (entity.type_) {
        case EntityType::Player:
            sound_effect = SoundEffect::AnimalCrush1;
            break;
        case EntityType::Bat:
            sound_effect = SoundEffect::AnimalCrush2;
            break;
        case EntityType::Gold:
        case EntityType::GoldStack:
            sound_effect = SoundEffect::MoneySmashed;
            break;
        case EntityType::Rock:
            sound_effect = SoundEffect::Thud;
            break;
        default:
            break;
        }
        if (sound_effect.has_value()) {
            audio.PlaySoundEffect(*sound_effect);
        }
    }
}

/** likely will need to add a condition for death immune objects */
void DieIfDead(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.health == 0) {
        entity.super_state = EntitySuperState::Dead;
        entity.state = EntityState::Dead;
        if (!entity.marked_for_destruction) {
            entity.display_state = EntityDisplayState::Dead;
        }
    }
    if (entity.super_state == EntitySuperState::Dead) {
        // DEATH SOUND
        // TODO, fill with the other entities
        // subdivide into categories of sound to reduce the burden here
        // allow for some randomness
        std::optional<SoundEffect> sound_effect;
        switch (entity.type_) {
        case EntityType::Bomb:
        case EntityType::JetPack:
            sound_effect = SoundEffect::BombExplosion;
            break;
        case EntityType::Pot:
            sound_effect = SoundEffect::PotShatter;
            break;
        case EntityType::Box:
            sound_effect = SoundEffect::BoxBreak;
            break;
        default:
            break;
        }
        if (sound_effect.has_value()) {
            audio.PlaySoundEffect(*sound_effect);
            entity.super_state = EntitySuperState::Dead;
        }
    }
}

void MaybeHurtAndStunOnContact(std::size_t entity_idx, State& state, Audio& audio) {
    // HURT and/or STUN ON CONTACT
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB entity_aabb = entity.GetAABB();
    const Vec2 entity_pos = entity.pos;
    const EntitySuperState super_state = entity.super_state;
    const bool hurt_on_contact = entity.hurt_on_contact;
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Alignment alignment = entity.alignment;
    if (super_state != EntitySuperState::Dead && super_state != EntitySuperState::Stunned &&
        hurt_on_contact) {
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
            if (thrown_by.has_value() && vid == *thrown_by) {
                continue;
            }
            if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
                // skip a player, if collision with bottom quarter of players body
                if (other_entity->type_ == EntityType::Player) {
                    const AABB player_aabb = other_entity->GetAABB();
                    const AABB player_foot = {
                        .tl = Vec2::New(player_aabb.tl.x, player_aabb.br.y - 4.0F),
                        .br = player_aabb.br,
                    };
                    // if entity in players foot, skip
                    if (entity_pos.x >= player_foot.tl.x && entity_pos.x <= player_foot.br.x &&
                        entity_pos.y >= player_foot.tl.y && entity_pos.y <= player_foot.br.y) {
                        continue;
                    }
                }
                if (other_entity->can_collide) {
                    const bool has_correct_alignment =
                        (alignment == Alignment::Ally && other_entity->alignment == Alignment::Enemy) ||
                        (alignment == Alignment::Enemy &&
                         other_entity->alignment == Alignment::Ally) ||
                        (alignment == Alignment::Neutral);
                    if (has_correct_alignment) {
                        const DamageResult damage_result =
                            TryToDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            break;
                        case DamageResult::None:
                            break;
                        }
                        // TODO: thud for all hits sounds stupid, maybe???
                        // TODO: dead things get hit over and over, this is why theres a knockback
                        // state.frame_pause += 1;

                        // TODO: make entities do damage on hurt
                    }
                }
            }
        }
    }
}

void MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(
    std::size_t entity_idx,
    State& state,
    Audio& audio
) {
    MaybeHurtAndStunOnContact(entity_idx, state, audio);
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Dead ||
        entity.super_state == EntitySuperState::Stunned) {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, audio);
    }
}

void ApplyHurtOnContact(std::size_t entity_idx, State& state, Audio& audio) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    //TODO: subdivide into living and nonliving or something to simplify this
    if (entity.hurt_on_contact) {
        MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(entity_idx, state, audio);
    } else if (entity.alignment == Alignment::Ally) {
    } else {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, audio);
    }
}

void DieIfFootInSpikes(std::size_t entity_idx, State& state, Audio& audio) {
    // if moving down and in the top portion of a spike tile, die
    Entity& entity = state.entity_manager.entities[entity_idx];
    bool fell_into_spikes = false;
    if (entity.vel.y > 0.0F) {
        const IAABB iaabb = entity.GetAABB().AsIAABB();
        const bool override_tile_portion_check = entity.vel.y > 4.0F;
        const bool in_top_portion_of_tile = (iaabb.br.y % static_cast<int>(kTileSize)) < 4;
        if (in_top_portion_of_tile || override_tile_portion_check) {
            const std::vector<const Tile*> tiles_in_body = state.stage.GetTilesInRectWc(iaabb.tl, iaabb.br);
            for (const Tile* tile : tiles_in_body) {
                if (*tile == Tile::Spikes) {
                    fell_into_spikes = true;
                }
            }
        }
    }
    if (fell_into_spikes) {
        const DamageResult damage_result =
            TryToDamageEntity(entity.vid.id, state, audio, DamageType::Spikes, 1);
        switch (damage_result) {
        case DamageResult::Hurt:
        case DamageResult::Died:
            entity.vel.x = 0.0F;
            audio.PlaySoundEffect(SoundEffect::AnimalCrush2);
            break;
        case DamageResult::None:
            break;
        }
    }
}

void DoDamagedEffect(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.last_health > entity.health) {
        switch (entity.type_) {
        case EntityType::Box:
        case EntityType::Pot:
            OnDamageEffectAsBreakawayContainer(entity_idx, state);
            break;
        case EntityType::Player:
        case EntityType::Bat:
            OnDamageEffectAsBleedingEntity(entity_idx, state);
            break;
        case EntityType::JetPack:
        case EntityType::Bomb:
            OnDeathEffectAsExplosive(entity_idx, state);
            break;
        default:
            break;
        }
    }
    entity.last_health = entity.health;
}

void DoSuperStateChangedEffect(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];

    // On Death
    if (entity.last_super_state != EntitySuperState::Dead &&
        entity.super_state == EntitySuperState::Dead) {
        switch (entity.type_) {
        case EntityType::JetPack:
        case EntityType::Bomb:
            OnDeathEffectAsExplosive(entity_idx, state);
            break;
        default:
            break;
        }
    }

    entity.last_super_state = entity.super_state;
}

bool IsGroundedOnEntities(std::size_t entity_idx, State& state) {
    // get the bounds of the line below the entities feet.
    const auto [entity_tl, entity_br] = state.entity_manager.entities[entity_idx].GetBounds();
    const VID vid = state.entity_manager.entities[entity_idx].vid;
    const Vec2 feet_tl = Vec2::New(entity_tl.x, entity_br.y);
    const Vec2 feet_br = entity_br + Vec2::New(0.0F, 1.0F);
    const AABB feet_aabb = {
        .tl = feet_tl,
        .br = feet_br,
    };
    const std::vector<VID> entities_at_feet =
        state.sid.QueryExclude(feet_aabb.tl, feet_aabb.br, vid);

    const bool impassable_entities = std::any_of(
        entities_at_feet.begin(),
        entities_at_feet.end(),
        [&](const VID& test_vid) { return state.entity_manager.entities[test_vid.id].impassable; });

    return impassable_entities;
}

SoundEffect GetProjectileCollisionSound(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
        return SoundEffect::PotShatter;
    default:
        return SoundEffect::Thud;
    }
}

bool MaybeHurtAndStunOnContactAsProjectile(std::size_t entity_idx, State& state, Audio& audio) {
    if (state.stage_frame < kStageSettleFrames) {
        return false;
    }
    bool hit = false;
    // HURT and/or STUN ON CONTACT
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB entity_aabb = entity.GetAABB();
    const EntityType entity_type = entity.type_;
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Vec2 entity_vel = entity.vel;

    // have to move fast enough
    if (Length(entity_vel) < 1.0F) {
        return false;
    }
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
        if (thrown_by.has_value() && vid == *thrown_by) {
            continue;
        }
        if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
            if (other_entity->can_collide) {
                hit = true;
                const DamageResult damage_result =
                    TryToDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                switch (damage_result) {
                case DamageResult::Hurt:
                case DamageResult::Died: {
                    const SoundEffect sound_effect = GetProjectileCollisionSound(entity_type);
                    audio.PlaySoundEffect(sound_effect);
                    break;
                }
                case DamageResult::None:
                    break;
                }
                // TODO: thud for all hits sounds stupid, maybe???
                // TODO: dead things get hit over and over, this is why theres a knockback

                // state.frame_pause += 1;
            }
        }
    }
    return hit;
}

} // namespace

void CommonStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    CrushIfCanBeCrushed(entity_idx, state, audio);
    DieIfDead(entity_idx, state, audio);
    StepStunTimer(entity_idx, state);
    DoThrownByStep(entity_idx, state); // TODO REVIEW THIS
    ApplyHurtOnContact(entity_idx, state, audio);
    DieIfFootInSpikes(entity_idx, state, audio);
    DoDamagedEffect(entity_idx, state, audio);
    DoSuperStateChangedEffect(entity_idx, state, audio);
    (void)graphics;
    (void)dt;
}

void CommonPostStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    StepAnimationTimer(entity_idx, state, graphics, dt);
    ApplyFrameDataGeometryToEntity(entity_idx, state, graphics);
}

void ApplyDeactivateConditions(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool vanish_on_death =
        entity.type_ == EntityType::Bomb || entity.type_ == EntityType::JetPack ||
        entity.type_ == EntityType::Pot || entity.type_ == EntityType::Box ||
        entity.type_ == EntityType::Rope;
    if ((vanish_on_death && entity.super_state == EntitySuperState::Dead) ||
        entity.marked_for_destruction) {
        state.entity_manager.SetInactive(entity_idx);
    }
}

void StoreHealthToLastHealth(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.last_health = entity.health;
}

void StepStunTimer(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Stunned) {
        if (entity.stun_timer == 0) {
            entity.super_state = EntitySuperState::Idle;
            entity.display_state = EntityDisplayState::Neutral;
        } else {
            // TODO: Loss of control setting was here.
            if (entity.stun_timer > 0) {
                entity.stun_timer -= 1;
            }
        }
    }
}

//TODO: default super state to display state routing function

void StepTravelSoundWalkerClimber(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.travel_sound_countdown -= entity.dist_traveled_this_frame;

    if (!entity.grounded && !entity.climbing) {
        return;
    }

    if (entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;
        // TODO: TRAVEL SOUND MATCH ON ENTITY TYPE
        //  match on entity type  lol so different travel sound per entity

        SoundEffect which_step_sound;
        if (entity.climbing) {
            const auto [entity_tl, entity_br] = entity.GetBounds();
            const std::vector<const Tile*> newly_collided_tiles =
                state.stage.GetTilesInRectWc(ToIVec2(entity_tl), ToIVec2(entity_br));
            bool its_rope = false;
            bool its_ladder = false;
            for (const Tile* tile : newly_collided_tiles) {
                if (*tile == Tile::Rope) {
                    its_rope = true;
                }
                if (*tile == Tile::Ladder) {
                    its_ladder = true;
                }
            }
            if (its_rope) {
                which_step_sound = entity.travel_sound == TravelSound::One
                                       ? SoundEffect::ClimbRope1
                                       : SoundEffect::ClimbRope2;
            } else if (its_ladder) {
                which_step_sound = entity.travel_sound == TravelSound::One
                                       ? SoundEffect::ClimbMetal1
                                       : SoundEffect::ClimbMetal2;
            } else {
                which_step_sound = SoundEffect::Step1;
            }
        } else {
            // walking/running
            which_step_sound =
                entity.travel_sound == TravelSound::One ? SoundEffect::Step1 : SoundEffect::Step2;
        }
        audio.PlaySoundEffect(which_step_sound);
        entity.IncTravelSound();
        // println!("{:?}: {}", entity.type_, entity.dist_traveled_this_frame);
    }
}

void StepAnimationTimer(std::size_t entity_idx, State& state, const Graphics& graphics, float dt) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = entity.display_state,
    });
    if (selection.has_value()) {
        entity.frame_data_animator.SetAnimation(selection->animation_id);
        entity.frame_data_animator.animate = selection->animate;
        if (selection->has_forced_frame) {
            entity.frame_data_animator.SetForcedFrame(selection->forced_frame);
        }
    }

    entity.frame_data_animator.Step(graphics.frame_data_db, dt);
}

void EulerStep(std::size_t entity_idx, State& state, float dt) {
    PrePartialEulerStep(entity_idx, state, dt);
    MoveEntityPixelStep(entity_idx, state, false, false);
    PostPartialEulerStep(entity_idx, state, dt);
}

void PrePartialEulerStep(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel += entity.acc;
}

void ApplyGravity(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.acc.y += state.stage.gravity;
}

void PostPartialEulerStep(std::size_t entity_idx, State& state, float dt) {
    (void)dt;
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.vel.x = std::clamp(entity.vel.x, -kMaxSpeed, kMaxSpeed);
    entity.vel.y = std::clamp(entity.vel.y, -kMaxSpeed, kMaxSpeed);
    entity.acc = Vec2::New(0.0F, 0.0F);
}

void ApplyGroundFriction(std::size_t entity_idx, State& state) {
    if (state.stage.stage_type == StageType::Ice1 || state.stage.stage_type == StageType::Ice2 ||
        state.stage.stage_type == StageType::Ice3) {
        return;
    }
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        entity.grounded = false;
    }

    if (IsGroundedOnEntities(entity_idx, state)) {
        state.entity_manager.entities[entity_idx].grounded |= true;
    }
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.SetGrounded(state.stage);
    if (entity.grounded) {
        entity.vel.x *= 0.85F;
    }
}

void GroundedCheck(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    bool check_tiles,
    bool check_entities
) {
    (void)audio;
    bool grounded = false;
    if (check_tiles) {
        grounded |= IsGroundedOnTiles(entity_idx, state);
    }
    if (check_entities) {
        grounded |= IsGroundedOnEntities(entity_idx, state);
    }

    Entity& entity = state.entity_manager.entities[entity_idx];

    entity.grounded = grounded;
    // entity.set_grounded(&stage);
    if (entity.grounded) {
        if (entity.vel.y > 0.0F) {
            entity.vel.y = 0.0F;
        }
        entity.coyote_time = 6;
    } else if (entity.coyote_time > 0) {
        entity.coyote_time -= 1;
    }
}

//** TODO: should be extracted out into a general entity thing? */
bool IsGroundedOnTiles(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];

    // get entity bounds
    const auto [entity_tl, entity_br] = entity.GetBounds();
    // check just below player
    const Vec2 feet_tl = Vec2::New(entity_tl.x, entity_br.y);
    const Vec2 feet_br = entity_br + Vec2::New(0.0F, 1.0F);
    if (feet_br.y >= static_cast<float>(state.stage.GetHeight())) {
        return true;
    }

    // get tiles in player bounds
    const IVec2 feet_tl_tile_pos = ToIVec2(feet_tl) / static_cast<int>(kTileSize);
    const IVec2 feet_br_tile_pos = ToIVec2(feet_br) / static_cast<int>(kTileSize);
    const std::vector<const Tile*> tiles_at_feet =
        state.stage.GetTilesInRect(feet_tl_tile_pos, feet_br_tile_pos);
    const bool collided = CollidableTileInList(tiles_at_feet);
    if (collided) {
        return true;
    }
    return false;
}

void DoThrownByStep(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const std::optional<VID> thrown_by = entity.thrown_by;
    if (thrown_by) {
        if (entity.thrown_immunity_timer > 0) {
            entity.thrown_immunity_timer -= 1;
        }
    }
    if (entity.thrown_immunity_timer == 0) {
        entity.thrown_by.reset();
    }
}

void HangHandsStep(std::size_t entity_idx, State& state) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID vid = entity.vid;
    const Vec2 vel = entity.vel;
    bool no_hang = entity.no_hang;
    if (entity.super_state == EntitySuperState::Stunned ||
        entity.super_state == EntitySuperState::Dead) {
        no_hang = true;
    }
    Stage& stage = state.stage;
    const HangHandBounds hanghands = entity.GetHangHandsBounds();

    bool left_hanging = false;
    bool right_hanging = false;
    if (!no_hang) {
        if (vel.y >= 0.0F) {
            const std::vector<const Tile*> hanghand_collided_tiles =
                stage.GetTilesInRectWc(ToIVec2(hanghands.left_tl), ToIVec2(hanghands.left_br));
            const bool on_collidable_tiles = CollidableTileInList(hanghand_collided_tiles);

            const std::vector<VID> entities_at_hang_hands =
                state.sid.QueryExclude(hanghands.left_tl, hanghands.left_br, vid);
            const bool on_impassable_entities = std::any_of(
                entities_at_hang_hands.begin(),
                entities_at_hang_hands.end(),
                [&](const VID& test_vid) {
                    return state.entity_manager.entities[test_vid.id].impassable;
                });

            const bool hh_blocked = on_collidable_tiles || on_impassable_entities;

            const Vec2 under_tl = Vec2::New(hanghands.left_tl.x, hanghands.left_br.y);
            const Vec2 under_br = hanghands.left_br + Vec2::New(0.0F, Entity::kHangHandSize.y);
            const std::vector<const Tile*> under_hanghand_collided_tiles =
                stage.GetTilesInRectWc(ToIVec2(under_tl), ToIVec2(under_br));
            const bool on_collidable_under_tiles =
                CollidableTileInList(under_hanghand_collided_tiles);

            const std::vector<VID> entities_under_hang_hands =
                state.sid.QueryExclude(under_tl, under_br, vid);
            const bool on_impassable_under_entities = std::any_of(
                entities_under_hang_hands.begin(),
                entities_under_hang_hands.end(),
                [&](const VID& test_vid) {
                    return state.entity_manager.entities[test_vid.id].impassable;
                });

            const bool uhh_blocked = on_collidable_under_tiles || on_impassable_under_entities;
            left_hanging = !hh_blocked && uhh_blocked;
        }

        if (vel.y >= 0.0F) {
            const std::vector<const Tile*> hanghand_collided_tiles =
                stage.GetTilesInRectWc(ToIVec2(hanghands.right_tl), ToIVec2(hanghands.right_br));
            const bool on_collidable_tiles = CollidableTileInList(hanghand_collided_tiles);

            const std::vector<VID> entities_at_hang_hands =
                state.sid.QueryExclude(hanghands.right_tl, hanghands.right_br, vid);
            const bool on_impassable_entities = std::any_of(
                entities_at_hang_hands.begin(),
                entities_at_hang_hands.end(),
                [&](const VID& test_vid) {
                    return state.entity_manager.entities[test_vid.id].impassable;
                });

            const bool hh_blocked = on_collidable_tiles || on_impassable_entities;

            const Vec2 under_tl = Vec2::New(hanghands.right_tl.x, hanghands.right_br.y);
            const Vec2 under_br = hanghands.right_br + Vec2::New(0.0F, Entity::kHangHandSize.y);
            const std::vector<const Tile*> under_hanghand_collided_tiles =
                stage.GetTilesInRectWc(ToIVec2(under_tl), ToIVec2(under_br));
            const bool on_collidable_under_tiles =
                CollidableTileInList(under_hanghand_collided_tiles);

            const std::vector<VID> entities_under_hang_hands =
                state.sid.QueryExclude(under_tl, under_br, vid);
            const bool on_impassable_under_entities = std::any_of(
                entities_under_hang_hands.begin(),
                entities_under_hang_hands.end(),
                [&](const VID& test_vid) {
                    return state.entity_manager.entities[test_vid.id].impassable;
                });

            const bool uhh_blocked = on_collidable_under_tiles || on_impassable_under_entities;
            right_hanging = !hh_blocked && uhh_blocked;
        }
    }

    Entity& mutable_entity = state.entity_manager.entities[entity_idx];
    mutable_entity.left_hanging = left_hanging;
    mutable_entity.right_hanging = right_hanging;

    if (mutable_entity.left_hanging || mutable_entity.right_hanging) {
        mutable_entity.grounded = false;
    }
    if (mutable_entity.no_hang) {
        mutable_entity.left_hanging = false;
        mutable_entity.right_hanging = false;
    }
    if (mutable_entity.IsHanging()) {
        mutable_entity.vel.y = 0.0F;
        mutable_entity.grounded = false;
    }
}

/** damaging an entity should always be done through this, unless you want to mow through the entities immunities...
 * all entities will be killed if their health is zero.
*/
DamageResult TryToDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount
) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];
    const DamageVulnerability v = entity.damage_vulnerability;
    if (v == DamageVulnerability::Immune) {
        return DamageResult::None;
    }
    bool can_damage = false;
    switch (entity.damage_vulnerability) {
    case DamageVulnerability::AttackingOnly:
        can_damage = damage_type == DamageType::Attack;
        break;
    case DamageVulnerability::BurningOnly:
        can_damage = damage_type == DamageType::Burn;
        break;
    case DamageVulnerability::CrushingOnly:
        can_damage = damage_type == DamageType::Crush;
        break;
    case DamageVulnerability::ExplosionOnly:
        can_damage = damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::CrushingAndSpikes:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes;
        break;
    case DamageVulnerability::CrushingSpikesAndExplosion:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes ||
                     damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::HeavyAttackOnly:
        can_damage = damage_type == DamageType::HeavyAttack;
        break;
    case DamageVulnerability::Vulnerable:
        can_damage = true;
        break;
    case DamageVulnerability::Immune:
        can_damage = false;
        break;
    case DamageVulnerability::AnthingExceptJumpOn:
        can_damage = damage_type != DamageType::JumpOn;
        break;
    }
    if (can_damage) {
        bool do_damage_calculation = false;
        if (damage_type == DamageType::Crush) {
            // TODO: does crushing even ever run
            entity.health = 0;
            entity.super_state = EntitySuperState::Crushed;
            return DamageResult::Died;
        }
        if (entity.super_state == EntitySuperState::Dead) {
            return DamageResult::None;
        } else {
            if (damage_type == DamageType::Spikes) {
                entity.health = 0;
                entity.super_state = EntitySuperState::Dead;
                return DamageResult::Died;
            } else if (damage_type == DamageType::Explosion) {
                do_damage_calculation = true;
                if (entity.super_state != EntitySuperState::Stunned) {
                    entity.super_state = EntitySuperState::Stunned;
                    entity.display_state = EntityDisplayState::Stunned;
                    entity.stun_timer = kDefaultStunTimer;
                }
            } else if (entity.can_be_stunned) {
                if (entity.super_state != EntitySuperState::Stunned) {
                    entity.super_state = EntitySuperState::Stunned;
                    entity.display_state = EntityDisplayState::Stunned;
                    entity.stun_timer = kDefaultStunTimer;
                    do_damage_calculation = true;
                } else {
                    return DamageResult::None;
                }
            } else {
                do_damage_calculation = true;
            }
        }
        if (do_damage_calculation) {
            if (entity.health > amount) {
                entity.health -= amount;
                return DamageResult::Hurt;
            }
            entity.health = 0;
            return DamageResult::Died;
        }
    }
    return DamageResult::None;
}

void JumpingAndClimbingStep(std::size_t entity_idx, State& state, Audio& audio) {
    // TODO: this is using the player climb speed, jump impulse, and jump delay only,
    //  FIX THAT. maybe just one climb speed is fine
    //  TODO: only uses a common stage gravity for some reason...
    GroundedCheck(entity_idx, state, audio, true, true);

    Entity& entity = state.entity_manager.entities[entity_idx];
    Stage& stage = state.stage;
    const auto [player_tl, player_br] = entity.GetBounds();
    const std::vector<const Tile*> newly_collided_tiles =
        stage.GetTilesInRectWc(ToIVec2(player_tl), ToIVec2(player_br));
    const bool can_climb = ClimbableTileInList(newly_collided_tiles);

        if (can_climb) {
        if (entity.trying_to_go_up) {
            entity.climbing = true;
            entity.grounded = false;
            entity.vel.y = -player::kClimbSpeed;
        } else if (entity.climbing && !entity.trying_to_go_down) {
            entity.vel.y = 0.0F;
        }
        if (entity.climbing && !entity.trying_to_go_up && entity.trying_to_go_down) {
            entity.vel.y = player::kClimbSpeed;
        }
        if (entity.climbing && entity.trying_to_jump && entity.trying_to_go_down) {
            entity.climbing = false;
        }
    } else {
        entity.climbing = false;
    }

    if (entity.climbing && entity.grounded) {
        entity.climbing = false;
    }

    if (entity.super_state == EntitySuperState::Stunned ||
        entity.super_state == EntitySuperState::Dead) {
        entity.climbing = false;
    }

    if (entity.jumping) {
        if ((entity.grounded && (entity.jump_delay_frame_count == 0)) || entity.coyote_time > 0) {
            entity.jumped_this_frame = true;
            entity.vel.y = -player::kJumpImpulse;
            entity.coyote_time = 0;
            entity.grounded = false;
            entity.jump_delay_frame_count = player::kJumpDelayFrames;
            audio.PlaySoundEffect(SoundEffect::Jump);
        }
        if (entity.IsHanging()) {
            entity.vel.y = -player::kJumpImpulse;
            entity.left_hanging = false;
            entity.right_hanging = false;
            entity.grounded = false;
        }
    }
    entity.jumping = false;

    // reset the jump delay frame count
    if (entity.jump_delay_frame_count > 0) {
        entity.jump_delay_frame_count -= 1;
    }

    if (!entity.climbing) {
        entity.acc.y += state.stage.gravity;
    }
}

void DoTileAndEntityCollisions(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];

    entity.collided_last_frame = entity.collided;
    entity.collided = false;
    MoveEntityPixelStep(entity_idx, state, true, true);
    entity.collided |= entity.grounded;
}

void DoTileCollisions(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.collided_last_frame = entity.collided;
    entity.collided = false;
    MoveEntityPixelStep(entity_idx, state, true, false);
}

void DoExplosion(std::size_t entity_idx, Vec2 center, float size, State& state, Audio& audio) {
    // play explosion sound
    audio.PlaySoundEffect(SoundEffect::BombExplosion);
    // delete tiles in your bomb range
    const float explosion_size = size * static_cast<float>(kTileSize);
    const AABB area = {
        .tl = center - (Vec2::New(1.0F, 1.0F) * explosion_size),
        .br = center + (Vec2::New(1.0F, 1.0F) * explosion_size),
    };
    state.stage.SetTilesInRectWc(area, Tile::Air);

    // delete impassable entities in bomb range
    const VID this_vid = state.entity_manager.GetVid(entity_idx);
    const std::vector<VID> results = state.sid.QueryExclude(area.tl, area.br, this_vid);
    for (const VID& vid : results) {
        bool impassable = false;
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            impassable = entity->impassable;
            TryToDamageEntity(vid.id, state, audio, DamageType::Explosion, 10);
        }
        if (impassable) {
            state.entity_manager.SetInactive(vid.id);
        }
    }
}

} // namespace splonks::entities::common
