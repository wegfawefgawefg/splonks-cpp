#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace splonks {

using FrameDataId = std::uint32_t;

constexpr FrameDataId kInvalidFrameDataId = 0;
constexpr FrameDataId kFrameDataFnvOffsetBasis32 = 2166136261U;
constexpr FrameDataId kFrameDataFnvPrime32 = 16777619U;

constexpr FrameDataId HashFrameDataIdConstexpr(std::string_view text) {
    FrameDataId hash = kFrameDataFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<FrameDataId>(static_cast<unsigned char>(character));
        hash *= kFrameDataFnvPrime32;
    }
    return hash;
}

inline FrameDataId HashFrameDataId(const std::string& text) {
    return HashFrameDataIdConstexpr(text);
}

namespace frame_data_ids {

constexpr FrameDataId NoSprite = HashFrameDataIdConstexpr("no_sprite");
constexpr FrameDataId PlayerStanding = HashFrameDataIdConstexpr("player_standing");
constexpr FrameDataId PlayerStandingHolding =
    HashFrameDataIdConstexpr("player_standing_holding");
constexpr FrameDataId PlayerWalking = HashFrameDataIdConstexpr("player_walking");
constexpr FrameDataId PlayerWalkHolding = HashFrameDataIdConstexpr("player_walk_holding");
constexpr FrameDataId PlayerClimbing = HashFrameDataIdConstexpr("player_climbing");
constexpr FrameDataId PlayerDead = HashFrameDataIdConstexpr("player_dead");
constexpr FrameDataId PlayerStunned = HashFrameDataIdConstexpr("player_stunned");
constexpr FrameDataId PlayerHanging = HashFrameDataIdConstexpr("player_hanging");
constexpr FrameDataId PlayerFalling = HashFrameDataIdConstexpr("player_falling");
constexpr FrameDataId Pot = HashFrameDataIdConstexpr("pot");
constexpr FrameDataId Box = HashFrameDataIdConstexpr("box");
constexpr FrameDataId BaseballBatSwing = HashFrameDataIdConstexpr("baseball_bat_swing");
constexpr FrameDataId GoldCoin = HashFrameDataIdConstexpr("gold_coin");
constexpr FrameDataId GoldStack = HashFrameDataIdConstexpr("gold_stack");
constexpr FrameDataId BigGoldStack = HashFrameDataIdConstexpr("big_gold_stack");
constexpr FrameDataId RopeBall = HashFrameDataIdConstexpr("rope_ball");
constexpr FrameDataId UnfoldingRope = HashFrameDataIdConstexpr("unfolding_rope");
constexpr FrameDataId Jetpack = HashFrameDataIdConstexpr("jetpack");
constexpr FrameDataId JetpackBack = HashFrameDataIdConstexpr("jetpack_back");
constexpr FrameDataId JetpackSide = HashFrameDataIdConstexpr("jetpack_side");
constexpr FrameDataId LiveGrenade = HashFrameDataIdConstexpr("live_grenade");
constexpr FrameDataId Grenade = HashFrameDataIdConstexpr("grenade");
constexpr FrameDataId HangingBat = HashFrameDataIdConstexpr("hanging_bat");
constexpr FrameDataId FlyingBat = HashFrameDataIdConstexpr("flying_bat");
constexpr FrameDataId DeadBat = HashFrameDataIdConstexpr("dead_bat");
constexpr FrameDataId Rock = HashFrameDataIdConstexpr("rock");
constexpr FrameDataId CaveBlock = HashFrameDataIdConstexpr("cave_block");
constexpr FrameDataId IceBlock = HashFrameDataIdConstexpr("ice_block");
constexpr FrameDataId JungleBlock = HashFrameDataIdConstexpr("jungle_block");
constexpr FrameDataId TempleBlock = HashFrameDataIdConstexpr("temple_block");
constexpr FrameDataId BossBlock = HashFrameDataIdConstexpr("boss_block");
constexpr FrameDataId HeartUiIcon = HashFrameDataIdConstexpr("heart_ui_icon");
constexpr FrameDataId GrenadeUiIcon = HashFrameDataIdConstexpr("grenade_ui_icon");
constexpr FrameDataId RopeUiIcon = HashFrameDataIdConstexpr("rope_ui_icon");
constexpr FrameDataId GoldIcon = HashFrameDataIdConstexpr("gold_icon");
constexpr FrameDataId ToolSlot1 = HashFrameDataIdConstexpr("tool_slot_1");
constexpr FrameDataId ToolSlot2 = HashFrameDataIdConstexpr("tool_slot_2");

} // namespace frame_data_ids

} // namespace splonks
