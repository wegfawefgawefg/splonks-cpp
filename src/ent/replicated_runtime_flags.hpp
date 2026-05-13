#pragma once

#include "ent.hpp"

#include <cstdint>

namespace splonks {

constexpr std::uint32_t kReplicatedEntFlagRenderEnabled = 1U << 0U;
constexpr std::uint32_t kReplicatedEntFlagVerticalFlip = 1U << 1U;
constexpr std::uint32_t kReplicatedEntFlagStone = 1U << 2U;
constexpr std::uint32_t kReplicatedEntFlagCanBePickedUp = 1U << 3U;
constexpr std::uint32_t kReplicatedEntFlagImpassable = 1U << 4U;
constexpr std::uint32_t kReplicatedEntFlagCanBeHungOn = 1U << 5U;
constexpr std::uint32_t kReplicatedEntFlagHurtOnContact = 1U << 6U;
constexpr std::uint32_t kReplicatedEntFlagVanishOnDeath = 1U << 7U;
constexpr std::uint32_t kReplicatedEntFlagCanGoOnBack = 1U << 8U;
constexpr std::uint32_t kReplicatedEntFlagCanHangLedge = 1U << 9U;
constexpr std::uint32_t kReplicatedEntFlagCanHangWall = 1U << 10U;
constexpr std::uint32_t kReplicatedEntFlagCanStomp = 1U << 11U;
constexpr std::uint32_t kReplicatedEntFlagCanBeStomped = 1U << 12U;
constexpr std::uint32_t kReplicatedEntFlagCanCollectPickups = 1U << 13U;
constexpr std::uint32_t kReplicatedEntFlagPushable = 1U << 14U;
constexpr std::uint32_t kReplicatedEntFlagCrusherPusher = 1U << 15U;
constexpr std::uint32_t kReplicatedEntFlagCanBeHit = 1U << 16U;
constexpr std::uint32_t kReplicatedEntFlagCanReceiveProjContact = 1U << 17U;
constexpr std::uint32_t kReplicatedEntFlagHolding = 1U << 18U;

inline std::uint32_t CaptureReplicatedRuntimeFlags(const Ent& ent) {
    std::uint32_t flags = 0;
    flags |= ent.render_enabled ? kReplicatedEntFlagRenderEnabled : 0U;
    flags |= ent.vertical_flip ? kReplicatedEntFlagVerticalFlip : 0U;
    flags |= ent.stone ? kReplicatedEntFlagStone : 0U;
    flags |= ent.can_be_picked_up ? kReplicatedEntFlagCanBePickedUp : 0U;
    flags |= ent.impassable ? kReplicatedEntFlagImpassable : 0U;
    flags |= ent.can_be_hung_on ? kReplicatedEntFlagCanBeHungOn : 0U;
    flags |= ent.hurt_on_contact ? kReplicatedEntFlagHurtOnContact : 0U;
    flags |= ent.vanish_on_death ? kReplicatedEntFlagVanishOnDeath : 0U;
    flags |= ent.can_go_on_back ? kReplicatedEntFlagCanGoOnBack : 0U;
    flags |= ent.can_hang_ledge ? kReplicatedEntFlagCanHangLedge : 0U;
    flags |= ent.can_hang_wall ? kReplicatedEntFlagCanHangWall : 0U;
    flags |= ent.can_stomp ? kReplicatedEntFlagCanStomp : 0U;
    flags |= ent.can_be_stomped ? kReplicatedEntFlagCanBeStomped : 0U;
    flags |= ent.can_collect_pickups ? kReplicatedEntFlagCanCollectPickups : 0U;
    flags |= ent.pushable ? kReplicatedEntFlagPushable : 0U;
    flags |= ent.crusher_pusher ? kReplicatedEntFlagCrusherPusher : 0U;
    flags |= ent.can_be_hit ? kReplicatedEntFlagCanBeHit : 0U;
    flags |= ent.can_receive_proj_contact ? kReplicatedEntFlagCanReceiveProjContact : 0U;
    flags |= ent.holding ? kReplicatedEntFlagHolding : 0U;
    return flags;
}

inline void ApplyReplicatedRuntimeFlags(Ent& ent, std::uint32_t flags) {
    ent.render_enabled = (flags & kReplicatedEntFlagRenderEnabled) != 0U;
    ent.vertical_flip = (flags & kReplicatedEntFlagVerticalFlip) != 0U;
    ent.stone = (flags & kReplicatedEntFlagStone) != 0U;
    ent.can_be_picked_up = (flags & kReplicatedEntFlagCanBePickedUp) != 0U;
    ent.impassable = (flags & kReplicatedEntFlagImpassable) != 0U;
    ent.can_be_hung_on = (flags & kReplicatedEntFlagCanBeHungOn) != 0U;
    ent.hurt_on_contact = (flags & kReplicatedEntFlagHurtOnContact) != 0U;
    ent.vanish_on_death = (flags & kReplicatedEntFlagVanishOnDeath) != 0U;
    ent.can_go_on_back = (flags & kReplicatedEntFlagCanGoOnBack) != 0U;
    ent.can_hang_ledge = (flags & kReplicatedEntFlagCanHangLedge) != 0U;
    ent.can_hang_wall = (flags & kReplicatedEntFlagCanHangWall) != 0U;
    ent.can_stomp = (flags & kReplicatedEntFlagCanStomp) != 0U;
    ent.can_be_stomped = (flags & kReplicatedEntFlagCanBeStomped) != 0U;
    ent.can_collect_pickups = (flags & kReplicatedEntFlagCanCollectPickups) != 0U;
    ent.pushable = (flags & kReplicatedEntFlagPushable) != 0U;
    ent.crusher_pusher = (flags & kReplicatedEntFlagCrusherPusher) != 0U;
    ent.can_be_hit = (flags & kReplicatedEntFlagCanBeHit) != 0U;
    ent.can_receive_proj_contact =
        (flags & kReplicatedEntFlagCanReceiveProjContact) != 0U;
    ent.holding = (flags & kReplicatedEntFlagHolding) != 0U;
}

} // namespace splonks
