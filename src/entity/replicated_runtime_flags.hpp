#pragma once

#include "entity.hpp"

#include <cstdint>

namespace splonks {

constexpr std::uint32_t kReplicatedEntityFlagRenderEnabled = 1U << 0U;
constexpr std::uint32_t kReplicatedEntityFlagVerticalFlip = 1U << 1U;
constexpr std::uint32_t kReplicatedEntityFlagStone = 1U << 2U;
constexpr std::uint32_t kReplicatedEntityFlagCanBePickedUp = 1U << 3U;
constexpr std::uint32_t kReplicatedEntityFlagImpassable = 1U << 4U;
constexpr std::uint32_t kReplicatedEntityFlagCanBeHungOn = 1U << 5U;
constexpr std::uint32_t kReplicatedEntityFlagHurtOnContact = 1U << 6U;
constexpr std::uint32_t kReplicatedEntityFlagVanishOnDeath = 1U << 7U;
constexpr std::uint32_t kReplicatedEntityFlagCanGoOnBack = 1U << 8U;
constexpr std::uint32_t kReplicatedEntityFlagCanHangLedge = 1U << 9U;
constexpr std::uint32_t kReplicatedEntityFlagCanHangWall = 1U << 10U;
constexpr std::uint32_t kReplicatedEntityFlagCanStomp = 1U << 11U;
constexpr std::uint32_t kReplicatedEntityFlagCanBeStomped = 1U << 12U;
constexpr std::uint32_t kReplicatedEntityFlagCanCollectPickups = 1U << 13U;
constexpr std::uint32_t kReplicatedEntityFlagPushable = 1U << 14U;
constexpr std::uint32_t kReplicatedEntityFlagCrusherPusher = 1U << 15U;
constexpr std::uint32_t kReplicatedEntityFlagCanBeHit = 1U << 16U;
constexpr std::uint32_t kReplicatedEntityFlagCanReceiveProjectileContact = 1U << 17U;

inline std::uint32_t CaptureReplicatedRuntimeFlags(const Entity& entity) {
    std::uint32_t flags = 0;
    flags |= entity.render_enabled ? kReplicatedEntityFlagRenderEnabled : 0U;
    flags |= entity.vertical_flip ? kReplicatedEntityFlagVerticalFlip : 0U;
    flags |= entity.stone ? kReplicatedEntityFlagStone : 0U;
    flags |= entity.can_be_picked_up ? kReplicatedEntityFlagCanBePickedUp : 0U;
    flags |= entity.impassable ? kReplicatedEntityFlagImpassable : 0U;
    flags |= entity.can_be_hung_on ? kReplicatedEntityFlagCanBeHungOn : 0U;
    flags |= entity.hurt_on_contact ? kReplicatedEntityFlagHurtOnContact : 0U;
    flags |= entity.vanish_on_death ? kReplicatedEntityFlagVanishOnDeath : 0U;
    flags |= entity.can_go_on_back ? kReplicatedEntityFlagCanGoOnBack : 0U;
    flags |= entity.can_hang_ledge ? kReplicatedEntityFlagCanHangLedge : 0U;
    flags |= entity.can_hang_wall ? kReplicatedEntityFlagCanHangWall : 0U;
    flags |= entity.can_stomp ? kReplicatedEntityFlagCanStomp : 0U;
    flags |= entity.can_be_stomped ? kReplicatedEntityFlagCanBeStomped : 0U;
    flags |= entity.can_collect_pickups ? kReplicatedEntityFlagCanCollectPickups : 0U;
    flags |= entity.pushable ? kReplicatedEntityFlagPushable : 0U;
    flags |= entity.crusher_pusher ? kReplicatedEntityFlagCrusherPusher : 0U;
    flags |= entity.can_be_hit ? kReplicatedEntityFlagCanBeHit : 0U;
    flags |= entity.can_receive_projectile_contact ? kReplicatedEntityFlagCanReceiveProjectileContact : 0U;
    return flags;
}

inline void ApplyReplicatedRuntimeFlags(Entity& entity, std::uint32_t flags) {
    entity.render_enabled = (flags & kReplicatedEntityFlagRenderEnabled) != 0U;
    entity.vertical_flip = (flags & kReplicatedEntityFlagVerticalFlip) != 0U;
    entity.stone = (flags & kReplicatedEntityFlagStone) != 0U;
    entity.can_be_picked_up = (flags & kReplicatedEntityFlagCanBePickedUp) != 0U;
    entity.impassable = (flags & kReplicatedEntityFlagImpassable) != 0U;
    entity.can_be_hung_on = (flags & kReplicatedEntityFlagCanBeHungOn) != 0U;
    entity.hurt_on_contact = (flags & kReplicatedEntityFlagHurtOnContact) != 0U;
    entity.vanish_on_death = (flags & kReplicatedEntityFlagVanishOnDeath) != 0U;
    entity.can_go_on_back = (flags & kReplicatedEntityFlagCanGoOnBack) != 0U;
    entity.can_hang_ledge = (flags & kReplicatedEntityFlagCanHangLedge) != 0U;
    entity.can_hang_wall = (flags & kReplicatedEntityFlagCanHangWall) != 0U;
    entity.can_stomp = (flags & kReplicatedEntityFlagCanStomp) != 0U;
    entity.can_be_stomped = (flags & kReplicatedEntityFlagCanBeStomped) != 0U;
    entity.can_collect_pickups = (flags & kReplicatedEntityFlagCanCollectPickups) != 0U;
    entity.pushable = (flags & kReplicatedEntityFlagPushable) != 0U;
    entity.crusher_pusher = (flags & kReplicatedEntityFlagCrusherPusher) != 0U;
    entity.can_be_hit = (flags & kReplicatedEntityFlagCanBeHit) != 0U;
    entity.can_receive_projectile_contact =
        (flags & kReplicatedEntityFlagCanReceiveProjectileContact) != 0U;
}

} // namespace splonks
