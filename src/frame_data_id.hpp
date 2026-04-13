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
constexpr FrameDataId SpiderHang = HashFrameDataIdConstexpr("spider_hang");
constexpr FrameDataId GiantSpiderHang = HashFrameDataIdConstexpr("giant_spider_hang");
constexpr FrameDataId Rock = HashFrameDataIdConstexpr("rock");
constexpr FrameDataId Chest = HashFrameDataIdConstexpr("chest");
constexpr FrameDataId ChestOpen = HashFrameDataIdConstexpr("chest_open");
constexpr FrameDataId KeyChest = HashFrameDataIdConstexpr("key_chest");
constexpr FrameDataId KeyChestOpen = HashFrameDataIdConstexpr("key_chest_open");
constexpr FrameDataId ChestKey = HashFrameDataIdConstexpr("chest_key");
constexpr FrameDataId UdjatEye = HashFrameDataIdConstexpr("udjat_eye");
constexpr FrameDataId Dice = HashFrameDataIdConstexpr("dice");
constexpr FrameDataId Damsel = HashFrameDataIdConstexpr("damsel");
constexpr FrameDataId Shopkeeper = HashFrameDataIdConstexpr("shopkeeper");
constexpr FrameDataId Caveman = HashFrameDataIdConstexpr("caveman");
constexpr FrameDataId Snake = HashFrameDataIdConstexpr("snake");
constexpr FrameDataId Scarab = HashFrameDataIdConstexpr("scarab");
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
constexpr FrameDataId GoldIdol = HashFrameDataIdConstexpr("gold_idol");
constexpr FrameDataId Mattock = HashFrameDataIdConstexpr("mattock");
constexpr FrameDataId CapePickup = HashFrameDataIdConstexpr("cape_pickup");
constexpr FrameDataId Shotgun = HashFrameDataIdConstexpr("shotgun");
constexpr FrameDataId Teleporter = HashFrameDataIdConstexpr("teleporter");
constexpr FrameDataId Gloves = HashFrameDataIdConstexpr("gloves");
constexpr FrameDataId Spectacles = HashFrameDataIdConstexpr("spectacles");
constexpr FrameDataId WebCannon = HashFrameDataIdConstexpr("web_cannon");
constexpr FrameDataId Pistol = HashFrameDataIdConstexpr("pistol");
constexpr FrameDataId GrenadeBoom = HashFrameDataIdConstexpr("grenade_boom");
constexpr FrameDataId BigExplosion = HashFrameDataIdConstexpr("big_explosion");
constexpr FrameDataId LittleExplosion = HashFrameDataIdConstexpr("little_explosion");
constexpr FrameDataId Spark = HashFrameDataIdConstexpr("spark");
constexpr FrameDataId Pow = HashFrameDataIdConstexpr("pow");
constexpr FrameDataId LittleSmoke = HashFrameDataIdConstexpr("little_smoke");
constexpr FrameDataId BigSmoke = HashFrameDataIdConstexpr("big_smoke");
constexpr FrameDataId BloodBall = HashFrameDataIdConstexpr("blood_ball");
constexpr FrameDataId Sparkle = HashFrameDataIdConstexpr("sparkle");
constexpr FrameDataId LittleBrownShard = HashFrameDataIdConstexpr("little_brown_shard");
constexpr FrameDataId Mitt = HashFrameDataIdConstexpr("mitt");
constexpr FrameDataId Paste = HashFrameDataIdConstexpr("paste");
constexpr FrameDataId SpringShoes = HashFrameDataIdConstexpr("spring_shoes");
constexpr FrameDataId SpikeShoes = HashFrameDataIdConstexpr("spike_shoes");
constexpr FrameDataId Machete = HashFrameDataIdConstexpr("machete");
constexpr FrameDataId BombBox = HashFrameDataIdConstexpr("bomb_box");
constexpr FrameDataId BombBag = HashFrameDataIdConstexpr("bomb_bag");
constexpr FrameDataId Bow = HashFrameDataIdConstexpr("bow");
constexpr FrameDataId Compass = HashFrameDataIdConstexpr("compass");
constexpr FrameDataId Parachute = HashFrameDataIdConstexpr("parachute");
constexpr FrameDataId RopePile = HashFrameDataIdConstexpr("rope_pile");
constexpr FrameDataId EmeraldBig = HashFrameDataIdConstexpr("emerald_big");
constexpr FrameDataId SapphireBig = HashFrameDataIdConstexpr("sapphire_big");
constexpr FrameDataId RubyBig = HashFrameDataIdConstexpr("ruby_big");
constexpr FrameDataId AltarLeft = HashFrameDataIdConstexpr("altar_left");
constexpr FrameDataId AltarRight = HashFrameDataIdConstexpr("altar_right");
constexpr FrameDataId SacAltarLeft = HashFrameDataIdConstexpr("sac_altar_left");
constexpr FrameDataId SacAltarRight = HashFrameDataIdConstexpr("sac_altar_right");
constexpr FrameDataId SignGeneral = HashFrameDataIdConstexpr("sign_general");
constexpr FrameDataId SignBomb = HashFrameDataIdConstexpr("sign_bomb");
constexpr FrameDataId SignWeapon = HashFrameDataIdConstexpr("sign_weapon");
constexpr FrameDataId SignRare = HashFrameDataIdConstexpr("sign_rare");
constexpr FrameDataId SignClothing = HashFrameDataIdConstexpr("sign_clothing");
constexpr FrameDataId SignCraps = HashFrameDataIdConstexpr("sign_craps");
constexpr FrameDataId SignKissing = HashFrameDataIdConstexpr("sign_kissing");
constexpr FrameDataId Lantern = HashFrameDataIdConstexpr("lantern");
constexpr FrameDataId LanternRed = HashFrameDataIdConstexpr("lantern_red");
constexpr FrameDataId ArrowTrap = HashFrameDataIdConstexpr("arrow_trap");
constexpr FrameDataId GiantTikiHead = HashFrameDataIdConstexpr("giant_tiki_head");
constexpr FrameDataId KaliHead = HashFrameDataIdConstexpr("kali_head");
constexpr FrameDataId KaliBody = HashFrameDataIdConstexpr("kali_body");

} // namespace frame_data_ids

} // namespace splonks
