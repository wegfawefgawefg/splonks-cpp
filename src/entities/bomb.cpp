#include "entities/bomb.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "gameplay_authority.hpp"
#include "state.hpp"

#include <cmath>

namespace splonks::entities::bomb {

namespace {

constexpr float kBombRotationDegreesPerPixel = 24.0F;
constexpr float kStickyBombFlag = 1.0F;
constexpr float kLitBombSelfLight = 0.2F;
constexpr float kLitBombLightStrength = 0.55F;
constexpr int kLitBombLightRadius = 5;
constexpr Color3 kLitBombLightColor = Color3::New(1.0F, 0.48F, 0.16F);

bool IsStickyBomb(const Entity& bomb) {
    return bomb.counter_b >= 0.5F;
}

FrameDataId GetBombIdleAnimation(const Entity& bomb) {
    return IsStickyBomb(bomb) ? frame_data_ids::StickyGrenade : frame_data_ids::Grenade;
}

FrameDataId GetBombLiveAnimation(const Entity& bomb) {
    return IsStickyBomb(bomb) ? frame_data_ids::StickyLiveGrenade : frame_data_ids::LiveGrenade;
}

void StickBombInPlace(Entity& bomb) {
    bomb.vel = Vec2::New(0.0F, 0.0F);
    bomb.acc = Vec2::New(0.0F, 0.0F);
    bomb.has_physics = false;
    bomb.thrown_by.reset();
    bomb.thrown_immunity_timer = 0;
    bomb.projectile_contact_timer = 0;
    bomb.can_apply_projectile_contact = false;
}

void UpdateStickyBombAttachment(Entity& bomb, State& state) {
    if (!IsStickyBomb(bomb) || !bomb.entity_a.has_value()) {
        return;
    }

    const Entity* const attached = state.entity_manager.GetEntity(*bomb.entity_a);
    if (attached == nullptr || !attached->active) {
        bomb.entity_a.reset();
        bomb.has_physics = true;
        return;
    }

    bomb.pos = attached->pos + Vec2::New(
        static_cast<float>(bomb.point_a.x),
        static_cast<float>(bomb.point_a.y)
    );
    bomb.vel = Vec2::New(0.0F, 0.0F);
    bomb.acc = Vec2::New(0.0F, 0.0F);
}

void UpdateBombRotation(Entity& bomb) {
    if (bomb.held_by_vid.has_value() || bomb.attachment_mode != AttachmentMode::None) {
        return;
    }
    if (std::abs(bomb.vel.x) < 0.01F) {
        return;
    }

    bomb.rotation = std::fmod(
        bomb.rotation + (bomb.vel.x * kBombRotationDegreesPerPixel),
        360.0F
    );
    if (bomb.rotation < 0.0F) {
        bomb.rotation += 360.0F;
    }
}

void UpdateBombFuseLight(Entity& bomb) {
    if (bomb.counter_a <= 0.0F) {
        bomb.self_light = 0.0F;
        bomb.light_strength = 0.0F;
        bomb.light_color = Color3::White();
        bomb.light_radius = 0;
        return;
    }

    bomb.self_light = kLitBombSelfLight;
    bomb.light_strength = kLitBombLightStrength;
    bomb.light_color = kLitBombLightColor;
    bomb.light_radius = kLitBombLightRadius;
}

} // namespace

extern const EntityArchetype kBombArchetype{
    .type_ = EntityType::Bomb,
    .size = Vec2::New(8.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsBomb,
    .on_use = OnUseAsBomb,
    .step_logic = StepEntityLogicAsBomb,
    .on_entity_contact = OnEntityContactAsBomb,
    .on_tile_contact = OnTileContactAsBomb,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Grenade),
};

void MarkBombSticky(Entity& bomb) {
    bomb.counter_b = kStickyBombFlag;
    SetAnimation(bomb, frame_data_ids::StickyGrenade);
}

void OnDeathAsBomb(std::size_t entity_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(entity_idx, state, audio);
}

void OnUseAsBomb(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Entity& bomb = state.entity_manager.entities[entity_idx];
    if (!bomb.use_state.pressed || bomb.counter_a > 0.0F) {
        return;
    }

    bomb.counter_a = 144.0F;
    SetAnimation(bomb, GetBombLiveAnimation(bomb));

    if (bomb.use_state.source == AttachmentMode::None) {
        StopUsingEntity(bomb);
    }
}

void StepEntityLogicAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& bomb = state.entity_manager.entities[entity_idx];
    UpdateStickyBombAttachment(bomb, state);

    if (!HasLocalGameplayAuthorityForEntity(state, bomb.vid)) {
        UpdateBombFuseLight(bomb);
        UpdateBombRotation(bomb);
        return;
    }

    // if bomb is in winding up
    // set animation and display state
    // start decrementing the counter
    if (bomb.counter_a > 0.0F) {
        bomb.counter_a -= 1.0F;
        if (bomb.counter_a <= 0.0F) {
            UpdateBombFuseLight(bomb);
            bomb.health = 0;
            common::DieIfDead(entity_idx, state, audio);
            return;
        }
    }

    UpdateBombFuseLight(bomb);
    UpdateBombRotation(bomb);
}

common::ContactResolution OnEntityContactAsBomb(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    Entity& bomb = state.entity_manager.entities[entity_idx];
    const Entity& other = state.entity_manager.entities[other_entity_idx];
    if (!IsStickyBomb(bomb) || bomb.entity_a.has_value() ||
        context.phase != common::ContactPhase::SweptEntered) {
        return {};
    }
    if (bomb.thrown_by.has_value() && other.vid == *bomb.thrown_by) {
        return {};
    }
    if (other.held_by_vid.has_value() && bomb.thrown_by.has_value() &&
        *other.held_by_vid == *bomb.thrown_by) {
        return {};
    }
    if (!other.can_collide || other.type_ == EntityType::Bomb) {
        return {};
    }

    bomb.entity_a = other.vid;
    bomb.point_a = IVec2::New(
        static_cast<int>(std::lround(bomb.pos.x - other.pos.x)),
        static_cast<int>(std::lround(bomb.pos.y - other.pos.y))
    );
    StickBombInPlace(bomb);
    return {};
}

common::ContactResolution OnTileContactAsBomb(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    Entity& bomb = state.entity_manager.entities[entity_idx];
    if (!IsStickyBomb(bomb) || bomb.entity_a.has_value() ||
        context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return {};
    }

    StickBombInPlace(bomb);
    return {.stop_sweep = true};
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::bomb
