#include "ents/shopkeeper.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "ents/common/common.hpp"
#include "ents/shop.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::ents::shopkeeper {

namespace {

constexpr float kShopkeeperJumpSpeedY = -5.0F;
constexpr float kShopkeeperMoveSpeedX = 1.5F;
constexpr float kShopkeeperRecoverPistolSpeedX = 1.8F;
constexpr float kShopkeeperMoveAcceleration = 0.24F;
constexpr float kShopkeeperRecoverAcceleration = 0.28F;
constexpr float kShopkeeperShootDistance = 160.0F;
constexpr float kShopkeeperSightVerticalTolerance = 20.0F;
constexpr float kShopkeeperJumpCooldownFrames = 20.0F;
constexpr float kShopkeeperShootCooldownFrames = 45.0F;
constexpr float kShopkeeperRecoverPistolJumpHeightThreshold = 8.0F;

std::optional<std::size_t> GetShopIdxForShopkeeper(const Ent& shopkeeper, const State& state) {
    if (!shopkeeper.ent_a.has_value()) {
        return std::nullopt;
    }
    if (shopkeeper.ent_a->id >= state.ents.ents.size()) {
        return std::nullopt;
    }

    const Ent& shop = state.ents.ents[shopkeeper.ent_a->id];
    if (!shop.active || shop.type_ != EntType::Shop) {
        return std::nullopt;
    }
    return shopkeeper.ent_a->id;
}

bool CanSeePlayerAhead(const Ent& shopkeeper, const State& state, const Graphics& graphics) {
    const Vec2 shopkeeper_center = shopkeeper.GetRenderCenter();
    const int direction = shopkeeper.facing == Side::Left ? -1 : 1;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
            continue;
        }
        const Vec2 player_center =
            GetNearestWorldPoint(state.stage, shopkeeper_center, player->GetRenderCenter());
        const Vec2 delta = player_center - shopkeeper_center;
        if (std::abs(delta.y) > kShopkeeperSightVerticalTolerance ||
            std::abs(delta.x) > kShopkeeperShootDistance) {
            continue;
        }
        if ((direction < 0 && delta.x >= 0.0F) || (direction > 0 && delta.x <= 0.0F)) {
            continue;
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
        if (hit.type == WorldRayHitType::Ent && hit.ent_vid.has_value() &&
            *hit.ent_vid == player->vid) {
            return true;
        }
    }
    return false;
}

bool SpawnShopkeeperPistolIntoHands(std::size_t ent_idx, State& state, const Graphics& graphics) {
    Ent& shopkeeper = state.ents.ents[ent_idx];
    if (shopkeeper.holding_vid.has_value() || shopkeeper.ent_b.has_value()) {
        return false;
    }

    Ent* const pistol = world_ops::SpawnEnt(state, EntType::Pistol, [&](Ent& spawned_pistol) {
        spawned_pistol.held_by_vid = shopkeeper.vid;
        spawned_pistol.attach_mode = AttachMode::Held;
        spawned_pistol.has_physics = false;
        spawned_pistol.can_collide = false;
        spawned_pistol.counter_b = 9999.0F;
        spawned_pistol.facing = shopkeeper.facing;
        spawned_pistol.SetRenderCenter(shopkeeper.GetRenderCenter() + Vec2::New(4.0F, 1.0F));
        shopkeeper.holding_vid = spawned_pistol.vid;
        shopkeeper.holding = true;
        shopkeeper.ent_b = spawned_pistol.vid;
        state.UpdateSidForEnt(spawned_pistol.vid.id, graphics);
    }, shopkeeper.vid);
    if (pistol == nullptr) {
        return false;
    }
    return true;
}

Ent* GetTrackedPistol(Ent& shopkeeper, State& state) {
    if (!shopkeeper.ent_b.has_value()) {
        return nullptr;
    }

    Ent* const pistol = state.ents.GetEntMut(*shopkeeper.ent_b);
    if (pistol == nullptr || !pistol->active || pistol->type_ != EntType::Pistol) {
        return nullptr;
    }
    return pistol;
}

void SyncHeldPistolToShopkeeper(Ent& shopkeeper, Ent& pistol, State& state, const Graphics& graphics) {
    pistol.has_physics = false;
    pistol.can_collide = false;
    pistol.held_by_vid = shopkeeper.vid;
    pistol.attach_mode = AttachMode::Held;
    pistol.facing = shopkeeper.facing;
    pistol.draw_layer = DrawLayer::Foreground;
    StopUsingEnt(pistol);

    const Vec2 hold_offset = Vec2::New(4.0F, 1.0F);
    const Vec2 held_pos_target =
        shopkeeper.facing == Side::Left
            ? shopkeeper.GetRenderCenter() + Vec2::New(-hold_offset.x, hold_offset.y)
            : shopkeeper.GetRenderCenter() + hold_offset;
    pistol.SetRenderCenter(held_pos_target);
    pistol.grounded = false;
    state.UpdateSidForEnt(pistol.vid.id, graphics);
}

bool IsShopkeeperBlockedMovingTowardPistol(
    const Ent& shopkeeper,
    int move_direction,
    const State& state,
    const Graphics& graphics
) {
    if (move_direction == 0) {
        return false;
    }

    sim::AABB next_aabb = common::GetContactAabbForEnt(shopkeeper, graphics);
    const sim::Scalar offset = sim::Scalar::from_int(move_direction);
    next_aabb.tl.x += offset;
    next_aabb.br.x += offset;
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(
        state,
        graphics,
        next_aabb,
        shopkeeper.vid
    );
}

bool TryRecoverDroppedPistol(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
) {
    Ent& shopkeeper = state.ents.ents[ent_idx];
    Ent* const pistol = GetTrackedPistol(shopkeeper, state);
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

    const Vec2 delta = GetNearestWorldDelta(state.stage, shopkeeper.GetRenderCenter(), pistol->GetRenderCenter());
    if (delta.x < 0.0F) {
        shopkeeper.facing = Side::Left;
    } else if (delta.x > 0.0F) {
        shopkeeper.facing = Side::Right;
    }

    const int move_direction = delta.x < 0.0F ? -1 : (delta.x > 0.0F ? 1 : 0);
    if (shopkeeper.grounded) {
        common::AccelerateHorizontallyTowardSpeed(
            shopkeeper,
            state,
            static_cast<float>(move_direction) * kShopkeeperRecoverPistolSpeedX,
            kShopkeeperRecoverAcceleration
        );
    }

    const bool pistol_above = delta.y < -kShopkeeperRecoverPistolJumpHeightThreshold;
    const bool blocked_ahead =
        shopkeeper.grounded &&
        IsShopkeeperBlockedMovingTowardPistol(shopkeeper, move_direction, state, graphics);
    if (shopkeeper.grounded && shopkeeper.counter_a <= 0.0F && (pistol_above || blocked_ahead)) {
        shopkeeper.vel.y = sim::ToSimScalar(kShopkeeperJumpSpeedY);
        shopkeeper.counter_a = kShopkeeperJumpCooldownFrames;
    }

    const sim::AABB shopkeeper_aabb = common::GetContactAabbForEnt(shopkeeper, graphics);
    const sim::AABB pistol_aabb = GetNearestWorldAabb(
        state.stage,
        shopkeeper_aabb.center(),
        common::GetContactAabbForEnt(*pistol, graphics)
    );
    if (!gfxp::aabbs_intersect(shopkeeper_aabb, pistol_aabb)) {
        return true;
    }

    shopkeeper.holding_vid = pistol->vid;
    shopkeeper.holding = true;
    SyncHeldPistolToShopkeeper(shopkeeper, *pistol, state, graphics);
    return false;
}

} // namespace

EntDamageEffectResult OnDamageAsShopkeeper(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    (void)damage_type;
    (void)amount;

    if (ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    const Ent& shopkeeper = state.ents.ents[ent_idx];
    if (!shopkeeper.active) {
        return EntDamageEffectResult::None;
    }

    if (damage_applied ||
        damage_type == DamageType::Attack ||
        damage_type == DamageType::IgnitingAttack ||
        damage_type == DamageType::HeavyAttack) {
        ents::shop::DisturbShopByVid(shopkeeper.ent_a, state, audio);
    }

    return EntDamageEffectResult::None;
}

void StepEntLogicAsShopkeeper(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Ent& shopkeeper = state.ents.ents[ent_idx];
    if (shopkeeper.condition != EntCondition::Normal) {
        return;
    }

    if (shopkeeper.counter_a > 0.0F) {
        shopkeeper.counter_a -= 1.0F;
    }
    if (shopkeeper.counter_b > 0.0F) {
        shopkeeper.counter_b -= 1.0F;
    }

    if (const std::optional<std::size_t> shop_idx = GetShopIdxForShopkeeper(shopkeeper, state)) {
        const Ent& shop = state.ents.ents[*shop_idx];
        if (shop.ai_state == EntAiState::Disturbed) {
            shopkeeper.wanted = true;
        }
    }

    if (!shopkeeper.wanted) {
        common::DecelerateHorizontallyToStop(shopkeeper, kShopkeeperMoveAcceleration);
        TrySetAnim(shopkeeper, EntDisplayState::Neutral);
        return;
    }

    if (SpawnShopkeeperPistolIntoHands(ent_idx, state, graphics)) {
        (void)PlayEntSoundEmitter(state, shopkeeper, audio_asset_ids::PistolUnholster);
    }
    if (TryRecoverDroppedPistol(ent_idx, state, graphics)) {
        SetMovementFlag(shopkeeper, EntMovementFlag::Running, true);
        SetMovementFlag(shopkeeper, EntMovementFlag::Walking, true);
        TrySetAnim(shopkeeper, EntDisplayState::Walk);
        return;
    }

    const Ent* const player = FindNearestPlayer(state, shopkeeper.GetRenderCenter(), false);
    if (player == nullptr || player->condition == EntCondition::Dead) {
        return;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, shopkeeper.GetRenderCenter(), player->GetRenderCenter());
    if (delta.x < 0.0F) {
        shopkeeper.facing = Side::Left;
    } else if (delta.x > 0.0F) {
        shopkeeper.facing = Side::Right;
    }

    if (shopkeeper.grounded && shopkeeper.counter_a <= 0.0F) {
        shopkeeper.vel.y = sim::ToSimScalar(kShopkeeperJumpSpeedY);
        common::AccelerateHorizontallyTowardSpeed(
            shopkeeper,
            state,
            delta.x < 0.0F ? -kShopkeeperMoveSpeedX : kShopkeeperMoveSpeedX,
            kShopkeeperMoveAcceleration
        );
        shopkeeper.counter_a = kShopkeeperJumpCooldownFrames;
    }

    if (shopkeeper.grounded) {
        common::AccelerateHorizontallyTowardSpeed(
            shopkeeper,
            state,
            delta.x < 0.0F ? -kShopkeeperMoveSpeedX : kShopkeeperMoveSpeedX,
            kShopkeeperMoveAcceleration
        );
    }

    SetMovementFlag(shopkeeper, EntMovementFlag::Running, true);
    SetMovementFlag(shopkeeper, EntMovementFlag::Walking, true);
    TrySetAnim(shopkeeper, EntDisplayState::Walk);

    if (shopkeeper.holding_vid.has_value()) {
        if (Ent* const pistol = state.ents.GetEntMut(*shopkeeper.holding_vid)) {
            SyncHeldPistolToShopkeeper(shopkeeper, *pistol, state, graphics);
            if (shopkeeper.counter_b <= 0.0F && CanSeePlayerAhead(shopkeeper, state, graphics)) {
                UseEnt(*pistol, shopkeeper.vid, AttachMode::Held);
                shopkeeper.counter_b = kShopkeeperShootCooldownFrames;
            }
        }
    }
}

extern const EntSpec kShopkeeperSpec{
    .type_ = EntType::Shopkeeper,
    .size = EntSpecSize(16.0F, 16.0F),
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .on_damage = OnDamageAsShopkeeper,
    .step_logic = StepEntLogicAsShopkeeper,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Shopkeeper),
};

} // namespace splonks::ents::shopkeeper
