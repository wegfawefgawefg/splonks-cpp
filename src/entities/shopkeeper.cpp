#include "entities/shopkeeper.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "entities/common/common.hpp"
#include "entities/shop.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::entities::shopkeeper {

namespace {

constexpr float kShopkeeperJumpSpeedY = -5.0F;
constexpr float kShopkeeperMoveSpeedX = 1.5F;
constexpr float kShopkeeperRecoverPistolSpeedX = 1.8F;
constexpr float kShopkeeperShootDistance = 160.0F;
constexpr float kShopkeeperSightVerticalTolerance = 20.0F;
constexpr float kShopkeeperJumpCooldownFrames = 20.0F;
constexpr float kShopkeeperShootCooldownFrames = 45.0F;
constexpr float kShopkeeperRecoverPistolJumpHeightThreshold = 8.0F;

std::optional<std::size_t> GetShopIdxForShopkeeper(const Entity& shopkeeper, const State& state) {
    if (!shopkeeper.entity_a.has_value()) {
        return std::nullopt;
    }
    if (shopkeeper.entity_a->id >= state.entity_manager.entities.size()) {
        return std::nullopt;
    }

    const Entity& shop = state.entity_manager.entities[shopkeeper.entity_a->id];
    if (!shop.active || shop.type_ != EntityType::Shop) {
        return std::nullopt;
    }
    return shopkeeper.entity_a->id;
}

bool CanSeePlayerAhead(const Entity& shopkeeper, const State& state, const Graphics& graphics) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return false;
    }

    const Vec2 shopkeeper_center = shopkeeper.GetCenter();
    const Vec2 player_center =
        GetNearestWorldPoint(state.stage, shopkeeper_center, player->GetCenter());
    const Vec2 delta = player_center - shopkeeper_center;
    if (std::abs(delta.y) > kShopkeeperSightVerticalTolerance ||
        std::abs(delta.x) > kShopkeeperShootDistance) {
        return false;
    }

    const int direction = shopkeeper.facing == LeftOrRight::Left ? -1 : 1;
    if ((direction < 0 && delta.x >= 0.0F) || (direction > 0 && delta.x <= 0.0F)) {
        return false;
    }

    const WorldRayHit hit = RaycastHorizontal(
        shopkeeper,
        shopkeeper_center,
        direction,
        static_cast<int>(std::abs(delta.x)),
        state,
        graphics,
        shopkeeper.vid
    );
    return hit.type == WorldRayHitType::Entity && hit.entity_vid.has_value() &&
           *hit.entity_vid == player->vid;
}

bool SpawnShopkeeperPistolIntoHands(std::size_t entity_idx, State& state, const Graphics& graphics) {
    Entity& shopkeeper = state.entity_manager.entities[entity_idx];
    if (shopkeeper.holding_vid.has_value() || shopkeeper.entity_b.has_value()) {
        return false;
    }

    const std::optional<VID> pistol_vid = state.entity_manager.NewEntity();
    if (!pistol_vid.has_value()) {
        return false;
    }

    Entity* const pistol = state.entity_manager.GetEntityMut(*pistol_vid);
    if (pistol == nullptr) {
        return false;
    }

    SetEntityAs(*pistol, EntityType::Pistol);
    pistol->held_by_vid = shopkeeper.vid;
    pistol->attachment_mode = AttachmentMode::Held;
    pistol->has_physics = false;
    pistol->can_collide = false;
    pistol->counter_b = 9999.0F;
    pistol->facing = shopkeeper.facing;
    pistol->SetCenter(shopkeeper.GetCenter() + Vec2::New(4.0F, 1.0F));
    shopkeeper.holding_vid = pistol->vid;
    shopkeeper.holding = true;
    shopkeeper.entity_b = pistol->vid;
    state.UpdateSidForEntity(pistol_vid->id, graphics);
    return true;
}

Entity* GetTrackedPistol(Entity& shopkeeper, State& state) {
    if (!shopkeeper.entity_b.has_value()) {
        return nullptr;
    }

    Entity* const pistol = state.entity_manager.GetEntityMut(*shopkeeper.entity_b);
    if (pistol == nullptr || !pistol->active || pistol->type_ != EntityType::Pistol) {
        return nullptr;
    }
    return pistol;
}

void SyncHeldPistolToShopkeeper(Entity& shopkeeper, Entity& pistol, State& state, const Graphics& graphics) {
    pistol.has_physics = false;
    pistol.can_collide = false;
    pistol.held_by_vid = shopkeeper.vid;
    pistol.attachment_mode = AttachmentMode::Held;
    pistol.facing = shopkeeper.facing;
    pistol.draw_layer = DrawLayer::Foreground;
    StopUsingEntity(pistol);

    const Vec2 hold_offset = Vec2::New(4.0F, 1.0F);
    const Vec2 held_pos_target =
        shopkeeper.facing == LeftOrRight::Left
            ? shopkeeper.GetCenter() + Vec2::New(-hold_offset.x, hold_offset.y)
            : shopkeeper.GetCenter() + hold_offset;
    pistol.SetCenter(held_pos_target);
    pistol.grounded = false;
    state.UpdateSidForEntity(pistol.vid.id, graphics);
}

bool IsShopkeeperBlockedMovingTowardPistol(
    const Entity& shopkeeper,
    int move_direction,
    const State& state,
    const Graphics& graphics
) {
    if (move_direction == 0) {
        return false;
    }

    AABB next_aabb = common::GetContactAabbForEntity(shopkeeper, graphics);
    next_aabb.tl.x += static_cast<float>(move_direction);
    next_aabb.br.x += static_cast<float>(move_direction);
    return AabbHitsBlockingWorldGeometryOrImpassableEntities(
        state,
        graphics,
        next_aabb,
        shopkeeper.vid
    );
}

bool TryRecoverDroppedPistol(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
) {
    Entity& shopkeeper = state.entity_manager.entities[entity_idx];
    Entity* const pistol = GetTrackedPistol(shopkeeper, state);
    if (pistol == nullptr) {
        return false;
    }
    if (shopkeeper.holding_vid.has_value() && *shopkeeper.holding_vid == pistol->vid) {
        SyncHeldPistolToShopkeeper(shopkeeper, *pistol, state, graphics);
        return false;
    }
    if (pistol->held_by_vid.has_value()) {
        return false;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, shopkeeper.GetCenter(), pistol->GetCenter());
    if (delta.x < 0.0F) {
        shopkeeper.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        shopkeeper.facing = LeftOrRight::Right;
    }

    const int move_direction = delta.x < 0.0F ? -1 : (delta.x > 0.0F ? 1 : 0);
    if (shopkeeper.grounded) {
        shopkeeper.vel.x = static_cast<float>(move_direction) * kShopkeeperRecoverPistolSpeedX;
    }

    const bool pistol_above = delta.y < -kShopkeeperRecoverPistolJumpHeightThreshold;
    const bool blocked_ahead =
        shopkeeper.grounded &&
        IsShopkeeperBlockedMovingTowardPistol(shopkeeper, move_direction, state, graphics);
    if (shopkeeper.grounded && shopkeeper.counter_a <= 0.0F && (pistol_above || blocked_ahead)) {
        shopkeeper.vel.y = kShopkeeperJumpSpeedY;
        shopkeeper.counter_a = kShopkeeperJumpCooldownFrames;
    }

    const AABB shopkeeper_aabb = common::GetContactAabbForEntity(shopkeeper, graphics);
    const AABB pistol_aabb = GetNearestWorldAabb(
        state.stage,
        shopkeeper.GetCenter(),
        common::GetContactAabbForEntity(*pistol, graphics)
    );
    if (!AabbsIntersect(shopkeeper_aabb, pistol_aabb)) {
        return true;
    }

    shopkeeper.holding_vid = pistol->vid;
    shopkeeper.holding = true;
    SyncHeldPistolToShopkeeper(shopkeeper, *pistol, state, graphics);
    return false;
}

} // namespace

EntityDamageEffectResult OnDamageAsShopkeeper(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)damage_type;
    (void)amount;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    const Entity& shopkeeper = state.entity_manager.entities[entity_idx];
    if (!shopkeeper.active) {
        return EntityDamageEffectResult::None;
    }

    if (damage_applied ||
        damage_type == DamageType::Attack ||
        damage_type == DamageType::IgnitingAttack ||
        damage_type == DamageType::HeavyAttack) {
        entities::shop::DisturbShopByVid(shopkeeper.entity_a, state, audio);
    }

    return EntityDamageEffectResult::None;
}

void StepEntityLogicAsShopkeeper(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Entity& shopkeeper = state.entity_manager.entities[entity_idx];
    if (shopkeeper.condition != EntityCondition::Normal) {
        return;
    }

    if (shopkeeper.counter_a > 0.0F) {
        shopkeeper.counter_a -= 1.0F;
    }
    if (shopkeeper.counter_b > 0.0F) {
        shopkeeper.counter_b -= 1.0F;
    }

    if (const std::optional<std::size_t> shop_idx = GetShopIdxForShopkeeper(shopkeeper, state)) {
        const Entity& shop = state.entity_manager.entities[*shop_idx];
        if (shop.ai_state == EntityAiState::Disturbed) {
            shopkeeper.wanted = true;
        }
    }

    if (!shopkeeper.wanted) {
        shopkeeper.vel.x = 0.0F;
        TrySetAnimation(shopkeeper, EntityDisplayState::Neutral);
        return;
    }

    if (SpawnShopkeeperPistolIntoHands(entity_idx, state, graphics)) {
        audio.PlaySoundEffect(SoundEffect::PistolUnholster);
    }
    if (TryRecoverDroppedPistol(entity_idx, state, graphics)) {
        SetMovementFlag(shopkeeper, EntityMovementFlag::Running, true);
        SetMovementFlag(shopkeeper, EntityMovementFlag::Walking, true);
        TrySetAnimation(shopkeeper, EntityDisplayState::Walk);
        return;
    }

    if (!state.player_vid.has_value()) {
        return;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, shopkeeper.GetCenter(), player->GetCenter());
    if (delta.x < 0.0F) {
        shopkeeper.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        shopkeeper.facing = LeftOrRight::Right;
    }

    if (shopkeeper.grounded && shopkeeper.counter_a <= 0.0F) {
        shopkeeper.vel.y = kShopkeeperJumpSpeedY;
        shopkeeper.vel.x = delta.x < 0.0F ? -kShopkeeperMoveSpeedX : kShopkeeperMoveSpeedX;
        shopkeeper.counter_a = kShopkeeperJumpCooldownFrames;
    }

    if (shopkeeper.grounded) {
        shopkeeper.vel.x = delta.x < 0.0F ? -kShopkeeperMoveSpeedX : kShopkeeperMoveSpeedX;
    }

    SetMovementFlag(shopkeeper, EntityMovementFlag::Running, true);
    SetMovementFlag(shopkeeper, EntityMovementFlag::Walking, true);
    TrySetAnimation(shopkeeper, EntityDisplayState::Walk);

    if (shopkeeper.holding_vid.has_value()) {
        if (Entity* const pistol = state.entity_manager.GetEntityMut(*shopkeeper.holding_vid)) {
            SyncHeldPistolToShopkeeper(shopkeeper, *pistol, state, graphics);
            if (shopkeeper.counter_b <= 0.0F && CanSeePlayerAhead(shopkeeper, state, graphics)) {
                UseEntity(*pistol, shopkeeper.vid, AttachmentMode::Held);
                shopkeeper.counter_b = kShopkeeperShootCooldownFrames;
            }
        }
    }
}

extern const EntityArchetype kShopkeeperArchetype{
    .type_ = EntityType::Shopkeeper,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 20,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .on_damage = OnDamageAsShopkeeper,
    .step_logic = StepEntityLogicAsShopkeeper,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Shopkeeper),
};

} // namespace splonks::entities::shopkeeper
